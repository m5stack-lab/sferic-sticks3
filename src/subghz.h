#pragma once

// SubGHz toolset — CC1101 on the RF Pack S3. ADR-0001 (catalog), ADR-0012
// (shared SPI). Compiled only under -DSFERIC_RF_PACK. Drivers are scaffolded
// from the datasheet and UNVERIFIED until the pack is in hand.

#include <cstdint>
#include <string>

#include "ui.h"

// SubGHz > Band Scan. Sweeps the 433 MHz ISM band, reads CC1101 RSSI at each
// step, shows an occupancy bar + peak. A = restart sweep, B-hold = back.
class SubGhzScanScreen : public Screen {
 public:
  static constexpr int kSteps = 20;

  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  int8_t rssi_[kSteps] = {0};
  int step_ = 0;
  int peakIdx_ = 0;
  bool detected_ = false;
  uint32_t lastStep_ = 0;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void draw();
};

// Headless one-shot for MCP (docs/mcp/subghz.md). Optional {"ms":N} budget.
// Returns {"error":"radio_absent"} when no CC1101 probed.
std::string subghzScanJson(const std::string& params);
