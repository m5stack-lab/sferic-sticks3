#pragma once

#include <cstdint>

#include "ui.h"

class NimBLEScan;

// Bluetooth -> BLE Scan. Continuous NimBLE active scan; live list of
// advertising devices (name / RSSI / address tail / service count),
// RSSI colour-coded. A = next, A-hold / B = clear, B-hold = back.
class BleScanScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  NimBLEScan* scan_ = nullptr;
  bool started_ = false;
  int sel_ = 0;
  int top_ = 0;
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  uint32_t lastLog_ = 0;
  void draw();
};
