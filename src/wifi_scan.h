#pragma once

#include <cstdint>

#include "ui.h"

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
