#pragma once

// 2.4 GHz toolset — nRF24L01+ on the RF Pack S3. ADR-0001 (catalog),
// ADR-0012 (shared SPI). Compiled only under -DSFERIC_RF_PACK. Register
// logic is from the datasheet; the scan (RPD) path is bench-confirmed, the
// promiscuous sniff config is UNVERIFIED (docs/hardware/rf-pack-s3.md).

#include <cstdint>
#include <string>

#include "stream_hub.h"  // StrikeEmit
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

// 2.4GHz > Sniff. Promiscuous-ish capture: 2-byte pseudo-address, CRC and
// auto-ack off, fixed 32-byte payload; drains the RX FIFO and shows a frame
// count + the last payload. A = next channel, B-hold = back.
class Nrf24SniffScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  uint8_t ch_ = 2;
  uint32_t frames_ = 0;
  uint8_t last_[32] = {0};
  uint8_t lastLen_ = 0;
  bool detected_ = false;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void draw();
};

// Headless streaming session (docs/mcp/nrf24.md): promiscuous capture, emits
// one strike per caught frame ({"payload":{"raw":{"encoding":"bytes-b64",…}}}).
// Optional {"ch":N} (default 2). See src/stream_hub.*.
bool nrf24SniffStreamStart(const std::string& params);
void nrf24SniffStreamPoll(const StrikeEmit& emit);
void nrf24SniffStreamStop();

// 2.4GHz > Waterfall. Time-scrolling carrier-detect spectrogram: every
// channel sampled a few times per row (nRF24 RPD), rows stacked so Wi-Fi /
// BLE / hopping traffic shows as vertical bands. Display-only. A = clear,
// B-hold = back.
class Nrf24WaterfallScreen : public Screen {
 public:
  static constexpr int kChannels = 126;
  static constexpr int kRows = 100;
  static constexpr int kDwell = 3;  // RPD samples per channel per row

  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  uint8_t hist_[kRows][kChannels];  // 0..kDwell hits, newest at head_
  uint8_t row_[kChannels];
  int head_ = 0;
  int ch_ = 0;
  bool detected_ = false;
  bool dirty_ = true;
  void draw();
};
