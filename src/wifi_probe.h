#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui.h"

// WiFi -> Probe / Beacon. Promiscuous capture of probe-request frames:
// which client is looking for which SSID. Auto-hops channels. A = scroll,
// A-hold = clear, B-hold = back.
class WifiProbeScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  struct Entry {
    uint8_t mac[6];
    std::string ssid;
    int8_t rssi;
    uint32_t count;
    uint32_t last;
  };
  std::vector<Entry> entries_;
  uint32_t total_ = 0;
  int top_ = 0;
  int ch_ = 1;
  uint32_t lastHop_ = 0;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void merge(const uint8_t* mac, const char* ssid, int8_t rssi, uint32_t now);
  void draw();
};
