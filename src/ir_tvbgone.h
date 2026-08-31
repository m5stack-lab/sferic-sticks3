#pragma once

#include <cstdint>

#include "confirm.h"
#include "ui.h"

// Infrared -> TV-B-Gone. Sweeps common TV power codes on the IR LED
// (GPIO 46). B arms, B again fires (A cancels) — a stray press won't blast.
// B-hold = back.
class TvBGoneScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  enum class State : uint8_t { Idle, Firing, Done };
  State state_ = State::Idle;
  Arm arm_;
  int idx_ = 0;
  bool dirty_ = true;
  uint32_t lastSend_ = 0;
  void draw();
};
