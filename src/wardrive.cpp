#include "wardrive.h"

#include <M5Unified.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

#include "hal.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kRowH = 12;
constexpr size_t kMax = 300;

const char* encStr(uint8_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA12";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2E";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA23";
    default: return "?";
  }
}
}  // namespace

void WardriveScreen::onEnter() {
  dirty_ = true;
  log_.clear();
  bearing_ = 0;
  lastImu_ = hal.nowMs();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  scanning_ = false;
  nextScanMs_ = hal.nowMs() + 400;  // let WiFi settle before the first sweep
}

void WardriveScreen::onExit() {
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
}

void WardriveScreen::exportCsv() {
  Serial.println("---CSV-BEGIN---");
  Serial.println("bssid,ssid,rssi,channel,enc,bearing_rel,uptime_ms");
  char b[128];
  for (const auto& e : log_) {
    snprintf(b, sizeof(b),
             "%02X%02X%02X%02X%02X%02X,%s,%d,%u,%s,%.0f,%u", e.bssid[0],
             e.bssid[1], e.bssid[2], e.bssid[3], e.bssid[4], e.bssid[5],
             e.ssid.c_str(), e.rssi, e.ch, encStr(e.enc), e.bearing,
             (unsigned)e.ms);
    Serial.println(b);
  }
  Serial.println("---CSV-END---");
}

void WardriveScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (b == BtnEvent::Click) {
    Serial.printf("[wardrive] export %u aps\n", (unsigned)log_.size());
    exportCsv();
  } else if (a == BtnEvent::Hold) {
    bearing_ = 0;
    dirty_ = true;
  }

  // integrate gyro yaw
  const uint32_t now = hal.nowMs();
  float gx = 0, gy = 0, gz = 0;
  M5.Imu.getGyro(&gx, &gy, &gz);
  bearing_ += gz * (now - lastImu_) / 1000.0f;
  lastImu_ = now;
  while (bearing_ < 0) bearing_ += 360.0f;
  while (bearing_ >= 360.0f) bearing_ -= 360.0f;

  if (!scanning_ && now >= nextScanMs_) {
    WiFi.scanDelete();
    WiFi.scanNetworks(/*async=*/true, /*hidden=*/true);
    scanning_ = true;
  }
  if (scanning_) {
    const int r = WiFi.scanComplete();
    if (r >= 0) {
      for (int i = 0; i < r; ++i) {
        const uint8_t* bssid = WiFi.BSSID(i);
        bool known = false;
        for (auto& e : log_)
          if (memcmp(e.bssid, bssid, 6) == 0) {
            e.rssi = WiFi.RSSI(i);
            known = true;
            break;
          }
        if (!known && log_.size() < kMax) {
          Entry e{};
          memcpy(e.bssid, bssid, 6);
          e.ssid = WiFi.SSID(i).c_str();
          e.rssi = WiFi.RSSI(i);
          e.ch = WiFi.channel(i);
          e.enc = WiFi.encryptionType(i);
          e.bearing = bearing_;
          e.ms = now;
          log_.push_back(std::move(e));
        }
      }
      Serial.printf("[wardrive] sweep +%d  total %u\n", r,
                    (unsigned)log_.size());
      scanning_ = false;
      nextScanMs_ = now + 1500;
      dirty_ = true;
    } else if (r == WIFI_SCAN_FAILED) {
      scanning_ = false;
      nextScanMs_ = now + 1000;
    }
  }

  if (now - lastDraw_ > 500) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = now;
  }
}

void WardriveScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.printf("Wardrive  %u aps  brg %.0f", (unsigned)log_.size(), bearing_);

  const int rows = (hal.h() - kTitleH - 10) / kRowH;
  const int start = log_.size() > (size_t)rows ? log_.size() - rows : 0;
  for (int i = 0; start + i < (int)log_.size() && i < rows; ++i) {
    const Entry& e = log_[start + i];
    g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    g.setCursor(2, kTitleH + 3 + i * kRowH);
    g.printf("%-13s %4d c%-2d %s",
             e.ssid.empty() ? "<hidden>" : e.ssid.substr(0, 13).c_str(),
             e.rssi, e.ch, encStr(e.enc));
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A(hold) zero-brg  B csv  B(hold) back");
}
