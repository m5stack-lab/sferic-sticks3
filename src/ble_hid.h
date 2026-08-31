#pragma once

#include <cstdint>

#include "ui.h"

// Bluetooth -> BLE HID. Advertises as a BLE keyboard; when a host pairs,
// B types the selected canned payload. A = cycle payload, B = type,
// B-hold = back (stops advertising). Authorized input testing only
// (ADR-0001 scope).
class BleHidScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  int payload_ = 0;
  bool dirty_ = true;
  bool sending_ = false;
  size_t sendPos_ = 0;
  uint32_t lastKey_ = 0;
  uint32_t lastDraw_ = 0;
  void draw();
};
