#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui.h"

// Infrared -> Protocol Decode. Continuous receive + decode, no storage —
// a "what remote is this" peek. Keeps a short rolling history. B-hold = back.
class IrDecodeScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  std::vector<std::string> hist_;
  uint32_t lastHit_ = 0;
  bool dirty_ = true;
  void draw();
};
