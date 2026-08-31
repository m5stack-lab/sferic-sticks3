#pragma once

#include <cstdint>

#include "ui.h"

// Infrared -> TV-B-Gone. Sweeps a small set of common TV power codes on the
// IR LED (GPIO 46). B = fire, B-hold = back.
class TvBGoneScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  enum class State : uint8_t { Idle, Firing, Done };
  State state_ = State::Idle;
  int idx_ = 0;
  bool dirty_ = true;
  uint32_t lastSend_ = 0;
  void draw();
};
