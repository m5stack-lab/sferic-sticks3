#include "wardrive.h"

#include <ArduinoJson.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

#include <cstdio>
#include <vector>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

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
  } else if (a == BtnEvent::Click && !log_.empty()) {
    sel_ = (sel_ + 1) % (int)log_.size();
    dirty_ = true;
  } else if (a == BtnEvent::Hold && !log_.empty()) {
    sel_ = (sel_ - 1 + (int)log_.size()) % (int)log_.size();
    dirty_ = true;
  }
  if (kit::marqueeAnimating()) dirty_ = true;

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
  char t[40];
  std::snprintf(t, sizeof(t), "Wardrive  brg %.0f", bearing_);
  kit::begin(t, toolsetAccent("wifi"));

  if (log_.empty()) {
    kit::headline("0 aps", theme().fgDim);
    kit::end("B csv   B- back");
    return;
  }
  if (sel_ >= (int)log_.size()) sel_ = 0;
  const Entry& e = log_[sel_];
  char sub[56];
  std::snprintf(sub, sizeof(sub), "%d dBm  ch%d  %s  b%.0f", e.rssi, e.ch,
                encStr(e.enc), e.bearing);
  kit::detail(e.ssid.empty() ? "<hidden>" : e.ssid.c_str(), sub, sel_ + 1,
              (int)log_.size());
  kit::end("< >   B csv   B- back");
}

// ---------------- headless streaming ----------------

namespace {
struct SeenAp {
  uint8_t bssid[6];
};
std::vector<SeenAp> g_wdSeen;
float g_wdBrg = 0;
uint32_t g_wdImu = 0, g_wdNext = 0;
bool g_wdScanning = false;
}  // namespace

bool wardriveStreamStart(const std::string&) {
  g_wdSeen.clear();
  g_wdBrg = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  g_wdScanning = false;
  g_wdImu = millis();
  g_wdNext = millis() + 400;
  return true;
}

void wardriveStreamPoll(const StrikeEmit& emit) {
  const uint32_t now = millis();
  float gx = 0, gy = 0, gz = 0;
  M5.Imu.getGyro(&gx, &gy, &gz);
  g_wdBrg += gz * (now - g_wdImu) / 1000.0f;
  g_wdImu = now;
  while (g_wdBrg < 0) g_wdBrg += 360.0f;
  while (g_wdBrg >= 360.0f) g_wdBrg -= 360.0f;

  if (!g_wdScanning && now >= g_wdNext) {
    WiFi.scanDelete();
    WiFi.scanNetworks(/*async=*/true, /*hidden=*/true);
    g_wdScanning = true;
  }
  if (!g_wdScanning) return;

  const int r = WiFi.scanComplete();
  if (r == WIFI_SCAN_FAILED) {
    g_wdScanning = false;
    g_wdNext = now + 1000;
    return;
  }
  if (r < 0) return;  // still running

  for (int i = 0; i < r; ++i) {
    const uint8_t* b = WiFi.BSSID(i);
    bool known = false;
    for (const auto& s : g_wdSeen)
      if (memcmp(s.bssid, b, 6) == 0) {
        known = true;
        break;
      }
    if (known || g_wdSeen.size() >= 512) continue;
    g_wdSeen.push_back(SeenAp{});
    memcpy(g_wdSeen.back().bssid, b, 6);
    char bs[18];
    std::snprintf(bs, sizeof(bs), "%02x:%02x:%02x:%02x:%02x:%02x", b[0], b[1],
                  b[2], b[3], b[4], b[5]);
    JsonDocument d;
    d["bssid"] = bs;
    d["ssid"] = WiFi.SSID(i);
    d["rssi"] = WiFi.RSSI(i);
    d["channel"] = WiFi.channel(i);
    d["enc"] = encStr(WiFi.encryptionType(i));
    d["bearing"] = g_wdBrg;
    emit("ap", d);
  }
  g_wdScanning = false;
  g_wdNext = now + 1500;
}

void wardriveStreamStop() {
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  g_wdSeen.clear();
}
