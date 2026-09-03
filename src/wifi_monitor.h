#pragma once

#include <cstdint>
#include <string>

#include "stream_hub.h"  // StrikeEmit
#include "ui.h"

// Headless streaming session (ADR-0003 §6): channel-hopping promiscuous
// capture, emits a "monitor" strike ~1/s with total/mgmt/data/ctrl counts,
// rate, and last RSSI. See src/stream.cpp.
bool wifiMonitorStreamStart(const std::string& params);
void wifiMonitorStreamPoll(const StrikeEmit& emit);
void wifiMonitorStreamStop();

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
