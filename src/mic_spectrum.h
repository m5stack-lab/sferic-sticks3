#pragma once

#include <cstdint>

#include "ui.h"

// Sensors -> Mic Spectrum. Records from the ES8311 mic, FFTs it, draws a
// live spectrum and the peak frequency. A = hold/resume, B-hold = back.
class MicSpectrumScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  bool held_ = false;
  bool dirty_ = true;
  float peakHz_ = 0;
  float peakMag_ = 0;
  uint32_t lastLog_ = 0;
  void draw();
};
