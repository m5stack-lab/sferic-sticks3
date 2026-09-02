#include "wifi_scan.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"

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
  const Theme& th = theme();
  if (rssi > -60) return th.ok;
  if (rssi > -75) return th.warn;
  return th.bad;
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
  if (b == BtnEvent::Click && st_ != State::Scanning) {
    startScan();
  } else if (st_ == State::Done && count_ > 0) {
    if (a == BtnEvent::Click) {
      sel_ = (sel_ + 1) % count_;
      dirty_ = true;
    } else if (a == BtnEvent::Hold) {
      sel_ = (sel_ - 1 + count_) % count_;
      dirty_ = true;
    }
  }
  if (kit::marqueeAnimating()) dirty_ = true;

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

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void WifiScanScreen::draw() {
  const char* title = st_ == State::Scanning ? "AP Scan  scanning..."
                      : st_ == State::Failed ? "AP Scan  failed"
                                             : "AP Scan";
  kit::begin(title, toolsetAccent("wifi"));

  if (st_ != State::Done || count_ == 0) {
    kit::headline(st_ == State::Scanning ? "..." : "0", theme().fgDim);
    kit::end("B rescan   B- back");
    return;
  }
  if (sel_ >= count_) sel_ = 0;

  String ssid = WiFi.SSID(sel_);
  if (ssid.isEmpty()) ssid = "<hidden>";
  const int rssi = WiFi.RSSI(sel_);
  char sub[48];
  std::snprintf(sub, sizeof(sub), "%d dBm  ch%d  %s", rssi, WiFi.channel(sel_),
                encTag(WiFi.encryptionType(sel_)));
  kit::detail(ssid.c_str(), sub, sel_ + 1, count_);
  kit::end("< >  B rescan   B- back");
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
