#pragma once

#include <cstdint>
#include <string>

#include "confirm.h"
#include "ui.h"

// Headless emit (behind emit_arm/emit_fire). arm brings up the BLE keyboard
// advertisement; fire types params {"text": "..."} if a host is paired.
// Returns {"typed":<n>} or {"error":"not_paired"}.
bool bleHidArm(const std::string& params);
std::string bleHidEmit(const std::string& params);

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
  Arm typeArm_;
  bool dirty_ = true;
  bool sending_ = false;
  size_t sendPos_ = 0;
  uint32_t lastKey_ = 0;
  uint32_t lastDraw_ = 0;
  void draw();
};
