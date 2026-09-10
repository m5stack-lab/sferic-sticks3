#pragma once

// SubGHz toolset — CC1101 on the RF Pack S3. ADR-0001 (catalog), ADR-0012
// (shared SPI). Compiled only under -DSFERIC_RF_PACK. The register-level
// helpers are the standard SmartRC / rc-switch CC1101 OOK config and are
// UNVERIFIED on this board — bench-check against docs/hardware/rf-pack-s3.md.

#include <cstdint>
#include <string>
#include <vector>

#include "confirm.h"
#include "stream_hub.h"  // StrikeEmit
#include "ui.h"

// ---------------------------------------------------------------- scan -------

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

// ------------------------------------------------------ capture / replay -----

// Max OOK transition timings held per capture (mark/space µs, [0] = mark).
constexpr int kSubGhzRawMax = 512;

// Headless capture — one raw OOK burst, non-emitting (plain run()). Params
// {"ms":N} (default 5000), {"mhz":F} (default 433.92). Returns a strike per
// docs/mcp/strike-format.md ({"payload":{"raw":{"encoding":"us-timings",…}}})
// or {"error":"timeout"} / {"error":"radio_absent"}.
std::string subghzCaptureJson(const std::string& params);

// Headless replay — emit (behind emit_arm / emit_fire). Params:
// {"raw":[us,…], "mhz":F?, "repeats":N?} or a full captured strike object
// ({"payload":{"raw":{"data":[…]}}}). Returns {"sent":bool,"frames":N}.
std::string subghzReplayEmit(const std::string& params);

// SubGHz > Learn / Replay. Captures one OOK burst on the CC1101, holds it as
// a strike, re-transmits it. A = arm / recapture; replay is two-step (B arms,
// B again fires, A cancels). B-hold = back. Mirrors IrScreen.
class SubGhzLearnScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  enum class Mode : uint8_t { Idle, Listening, Captured };
  Mode mode_ = Mode::Idle;
  Arm replayArm_;
  bool detected_ = false;
  bool dirty_ = true;

  struct Strike {
    std::vector<uint16_t> raw;  // µs mark/space, [0] = mark
    uint32_t code = 0;          // decoded value if it framed, else 0
    uint8_t bits = 0;
    uint16_t te = 0;            // base pulse width µs (0 = undecoded)
    char summary[48] = {0};
  } strike_;

  void arm();
  void replay();
  void draw();
};

// ------------------------------------------------------------- decode --------

// SubGHz > Protocol Decode. Continuous OOK framing (PT2262 / EV1527 class),
// no storage — a "what's transmitting" peek. B-hold = back.
class SubGhzDecodeScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  std::vector<std::string> hist_;
  uint32_t lastHit_ = 0;
  bool detected_ = false;
  bool dirty_ = true;
  void draw();
};

// Headless streaming session (docs/mcp/subghz.md): continuous OOK decode,
// emits a "code" strike per framed packet. See src/stream_hub.*.
bool subghzDecodeStreamStart(const std::string& params);
void subghzDecodeStreamPoll(const StrikeEmit& emit);
void subghzDecodeStreamStop();

// ---------------------------------------------------------- fox hunt -------

// SubGHz > Fox Hunt. Live RSSI meter on one fixed frequency for walking down
// a transmitter — big dBm number, rolling graph, peak-hold. A cycles a
// frequency preset (433.92 / 315 / 868.35 / 915 / 433.42), B-hold exits.
class SubGhzFoxhuntScreen : public Screen {
 public:
  static constexpr int kGraph = 116;

  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  int8_t hist_[kGraph];
  int head_ = 0;
  int8_t cur_ = -120;
  int8_t peak_ = -120;
  uint8_t preset_ = 0;
  bool detected_ = false;
  uint32_t lastSample_ = 0;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void draw();
};

// Headless one-shot (docs/mcp/subghz.md): sample RSSI on {mhz?} (default
// 433.92) for {ms?} (default 3000). Returns
// {mhz, samples, rssi:{now,min,max,mean}}. radio_absent / radio_busy on error.
std::string subghzFoxhuntJson(const std::string& params);

// ---------------------------------------------------------- waterfall -------

// SubGHz > Waterfall. Time-scrolling 433 MHz RSSI spectrogram — the same
// CC1101 sweep as Band Scan, stacked as rows so bursts show as streaks.
// Display-only (no headless). A = cycle contrast, B-hold = back.
class SubGhzWaterfallScreen : public Screen {
 public:
  static constexpr int kBins = 60;
  static constexpr int kRows = 52;

  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  int8_t hist_[kRows][kBins];  // dBm per bin, newest at head_
  int head_ = 0;
  int bin_ = 0;
  int8_t row_[kBins];
  uint8_t gain_ = 1;  // contrast multiplier 1..3
  bool detected_ = false;
  uint32_t lastStep_ = 0;
  bool dirty_ = true;
  void draw();
};
