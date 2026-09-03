#pragma once

// 2.4 GHz toolset — nRF24L01+ on the RF Pack S3. ADR-0001 (catalog),
// ADR-0012 (shared SPI). Compiled only under -DSFERIC_RF_PACK. Scaffolded
// from the datasheet and UNVERIFIED until the pack is in hand.

#include <cstdint>
#include <string>

#include "ui.h"

// 2.4GHz > Band Scan. Poor-man's spectrum: per-channel carrier detect via
// the nRF24 RPD bit, accumulated over sweeps. A = clear, B-hold = back.
class Nrf24ScanScreen : public Screen {
 public:
  static constexpr int kChannels = 126;

  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  uint8_t hits_[kChannels] = {0};
  int ch_ = 0;
  int peakCh_ = 0;
  uint32_t sweeps_ = 0;
  bool detected_ = false;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void draw();
};

// Headless one-shot for MCP (docs/mcp/nrf24.md). Optional {"ms":N} budget.
// Returns {"error":"radio_absent"} when no nRF24 probed.
std::string nrf24ScanJson(const std::string& params);
