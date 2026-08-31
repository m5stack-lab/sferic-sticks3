#pragma once

#include <cstdint>

#include "ui.h"

// Sensors -> Gesture. Live BMI270 orientation + tilt + shake detection.
// A = reset shake count, B-hold = back.
class GestureScreen : public Screen {
 public:
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  uint32_t shakes_ = 0;
  uint32_t lastShake_ = 0;
  float ax_ = 0, ay_ = 0, az_ = 0, gmag_ = 0;
  const char* orient_ = "?";
  const char* gesture_ = "-";
  uint32_t lastGesture_ = 0;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void draw();
};
