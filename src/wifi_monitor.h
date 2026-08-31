#pragma once

#include <cstdint>

#include "ui.h"

// WiFi -> Packet Monitor. Promiscuous 802.11 capture: live frame counts by
// type, rate, last RSSI. A = step channel, A-hold = toggle hop, B = reset
// counters, B-hold = back.
class WifiMonitorScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  int ch_ = 1;
  bool hop_ = false;
  bool dirty_ = true;
  uint32_t rate_ = 0;
  uint32_t lastTotal_ = 0;
  uint32_t lastRate_ = 0;
  uint32_t lastHop_ = 0;
  uint32_t lastDraw_ = 0;
  void draw();
};
