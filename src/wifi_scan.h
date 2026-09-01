#pragma once

#include <cstdint>
#include <string>

#include "ui.h"

// Headless one-shot AP scan (ADR-0004 §4). Blocking for up to `timeoutMs`.
// Returns {"aps":[{ssid,bssid,rssi,channel,enc}], "count":N} or
// {"error":"radio_busy"|"timeout"|"scan_failed"}. Refuses while the Web UI
// SoftAP is up — over the collector link the radio is free.
std::string wifiScanJson(uint32_t timeoutMs = 4000);

// WiFi -> AP Scan. Async 2.4 GHz scan; lists SSID / RSSI / channel /
// encryption, colour-coded by signal. A = next row, A-hold / B = rescan,
// B-hold = back.
class WifiScanScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  enum class State : uint8_t { Idle, Scanning, Done, Failed };
  State st_ = State::Idle;
  int count_ = 0;
  int sel_ = 0;
  int top_ = 0;
  bool dirty_ = true;
  void startScan();
  void draw();
};
