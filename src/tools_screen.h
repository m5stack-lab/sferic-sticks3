#pragma once

#include "ui.h"

// Settings > Tools — the on-device control surface for ADR-0004. Lists every
// registered tool with an on/off box. A = next, B = toggle (persists to
// NVS), B-hold = back. Disabled tools drop out of the menu on the next visit.
class ToolsScreen : public Screen {
 public:
  void onEnter() override { dirty_ = true; }
  void tick() override;

 private:
  int sel_ = 0;
  int top_ = 0;
  bool dirty_ = true;
  void draw();
};
