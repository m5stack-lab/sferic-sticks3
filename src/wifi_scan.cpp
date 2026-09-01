#include "wifi_scan.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "hal.h"

namespace {

constexpr int kTitleH = 18;
constexpr int kRowH = 12;

const char* encTag(wifi_auth_mode_t m) {
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

uint16_t rssiColor(int rssi) {
  if (rssi > -60) return TFT_GREEN;
  if (rssi > -75) return TFT_YELLOW;
  return TFT_RED;
}

int visibleRows() { return (hal.h() - kTitleH - 10) / kRowH; }

}  // namespace

void WifiScanScreen::onEnter() {
  dirty_ = true;
  if (st_ == State::Idle) startScan();
}

void WifiScanScreen::onExit() {
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
}

void WifiScanScreen::startScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  WiFi.scanDelete();
  WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/true);
  st_ = State::Scanning;
  count_ = 0;
  sel_ = 0;
  top_ = 0;
  dirty_ = true;
}

void WifiScanScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Hold) {
    startScan();
  } else if (b == BtnEvent::Click && st_ != State::Scanning) {
    startScan();
  } else if (a == BtnEvent::Click && st_ == State::Done && count_ > 0) {
    sel_ = (sel_ + 1) % count_;
    dirty_ = true;
  }

  if (st_ == State::Scanning) {
    const int r = WiFi.scanComplete();
    if (r == WIFI_SCAN_RUNNING) {
      static uint32_t last = 0;
      if (hal.nowMs() - last > 250) {
        last = hal.nowMs();
        dirty_ = true;  // animate the "scanning" title
      }
    } else if (r == WIFI_SCAN_FAILED) {
      st_ = State::Failed;
      dirty_ = true;
    } else {
      count_ = r;
      st_ = State::Done;
      dirty_ = true;
      Serial.printf("[wifi] scan done: %d APs\n", count_);
    }
  }

  const int rows = visibleRows();
  if (sel_ < top_) {
    top_ = sel_;
    dirty_ = true;
  }
  if (sel_ >= top_ + rows) {
    top_ = sel_ - rows + 1;
    dirty_ = true;
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void WifiScanScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);

  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  switch (st_) {
    case State::Scanning: {
      const char dots[] = {'.', ' ', ' '};
      const int n = (hal.nowMs() / 250) % 3;
      g.printf("WiFi  scanning%c%c%c", dots[(0 + 3 - n) % 3],
               dots[(1 + 3 - n) % 3], dots[(2 + 3 - n) % 3]);
      break;
    }
    case State::Failed:
      g.print("WiFi  scan failed");
      break;
    default:
      g.printf("WiFi  %d APs", count_);
      break;
  }

  if (st_ == State::Done) {
    const int rows = visibleRows();
    for (int i = 0; i < rows && (top_ + i) < count_; ++i) {
      const int idx = top_ + i;
      const int y = kTitleH + i * kRowH;
      const bool onSel = (idx == sel_);
      if (onSel) g.fillRect(0, y, hal.w(), kRowH, TFT_NAVY);

      const int rssi = WiFi.RSSI(idx);
      g.setTextColor(onSel ? TFT_WHITE : rssiColor(rssi),
                     onSel ? TFT_NAVY : TFT_BLACK);
      g.setCursor(2, y + 2);

      String ssid = WiFi.SSID(idx);
      if (ssid.isEmpty()) ssid = "<hidden>";
      if (ssid.length() > 15) ssid = ssid.substring(0, 15);
      g.printf("%-15s%4d c%-2d %s", ssid.c_str(), rssi, WiFi.channel(idx),
               encTag(WiFi.encryptionType(idx)));
    }
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A next  A(hold)/B rescan  B(hold) back");
}

// ---------------- headless ----------------

std::string wifiScanJson(uint32_t timeoutMs) {
  if (WiFi.getMode() & WIFI_MODE_AP)
    return R"({"error":"radio_busy"})";

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  WiFi.scanDelete();
  WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/true);

  const uint32_t start = millis();
  int n = WIFI_SCAN_RUNNING;
  while ((n = WiFi.scanComplete()) == WIFI_SCAN_RUNNING) {
    if (millis() - start > timeoutMs) {
      WiFi.scanDelete();
      WiFi.mode(WIFI_OFF);
      return R"({"error":"timeout"})";
    }
    delay(50);
  }
  if (n < 0) {
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    return R"({"error":"scan_failed"})";
  }

  JsonDocument d;
  d["count"] = n;
  JsonArray aps = d["aps"].to<JsonArray>();
  for (int i = 0; i < n; ++i) {
    JsonObject o = aps.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["bssid"] = WiFi.BSSIDstr(i);
    o["rssi"] = WiFi.RSSI(i);
    o["channel"] = WiFi.channel(i);
    o["enc"] = encTag(WiFi.encryptionType(i));
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);

  std::string out;
  serializeJson(d, out);
  return out;
}
