#pragma once

#include <cstdint>
#include <vector>

#include "confirm.h"
#include "ui.h"

// Infrared -> Learn / Replay. Receives one IR signal on GPIO 42, holds it as
// a strike, re-transmits on GPIO 46. A = arm / recapture; replay is two-step
// (B arms, B again fires, A cancels). B-hold = back.
class IrScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  enum class Mode : uint8_t { Idle, Listening, Captured };
  Mode mode_ = Mode::Idle;
  Arm replayArm_;
  bool dirty_ = true;

  // The held strike (see docs/mcp/strike-format.md).
  struct Strike {
    int proto = -1;           // decode_type_t; -1 = none, 0 = UNKNOWN
    uint64_t value = 0;
    uint16_t bits = 0;
    std::vector<uint16_t> raw;  // µs mark/space timings (carrier 38k)
    char summary[48] = {0};
  } strike_;

  void arm();
  void replay();
  void draw();
};
