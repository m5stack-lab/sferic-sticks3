#pragma once

#include <cstdint>

#include "ui.h"

// Sensors -> Motion / Tamper. Watches the BMI270; flags movement above a
// sensitivity threshold and counts events. A = cycle sensitivity, B = reset
// count, B-hold = back.
class MotionScreen : public Screen {
 public:
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  int sens_ = 1;  // 0 low, 1 med, 2 high
  uint32_t events_ = 0;
  uint32_t lastTrig_ = 0;
  uint32_t lastDraw_ = 0;
  float accelDev_ = 0;
  float gyroMag_ = 0;
  bool dirty_ = true;
  void draw();
};
