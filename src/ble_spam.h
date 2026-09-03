#pragma once

#include <cstdint>
#include <string>

#include "stream_hub.h"  // StrikeEmit
#include "ui.h"

class NimBLEScan;

// Bluetooth -> Spam Detect. Passive scan with duplicates on; watches ADV
// packet rate and address churn. Flags BLE advertising floods
// (Flipper / continuity-spam style). B = reset peak, B-hold = back.
class BleSpamScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  NimBLEScan* scan_ = nullptr;
  bool started_ = false;
  uint32_t winStart_ = 0;
  uint32_t rate_ = 0;
  uint32_t churn_ = 0;
  uint32_t peakChurn_ = 0;
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  void draw();
};

// Headless streaming (ADR-0003 §6): the same passive adv-rate / churn watch,
// emitting a "spam" strike ~1/s with {rate, churn, verdict}.
bool bleSpamStreamStart(const std::string& params);
void bleSpamStreamPoll(const StrikeEmit& emit);
void bleSpamStreamStop();
