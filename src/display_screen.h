#pragma once

#include "ui.h"

// Settings > Display — screen-sleep timeout and awake brightness. Both persist
// to NVS via the HAL. A = next row, A(hold) = prev, B = cycle the selected
// value, B(hold) = back. Sleep only blanks the backlight; running tools and
// the web server keep ticking with the screen off.
class DisplayScreen : public Screen {
 public:
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  int sel_ = 0;  // 0 = sleep timeout, 1 = brightness
  bool dirty_ = true;
  void draw();
};
