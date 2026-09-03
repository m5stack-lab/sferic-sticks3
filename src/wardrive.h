#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stream_hub.h"  // StrikeEmit
#include "ui.h"

// Headless streaming (ADR-0003 §6): channel-wide async AP discovery + a
// gyro-integrated relative bearing; emits an "ap" strike per newly-seen
// BSSID with {bssid,ssid,rssi,channel,enc,bearing}.
bool wardriveStreamStart(const std::string& params);
void wardriveStreamPoll(const StrikeEmit& emit);
void wardriveStreamStop();

// WiFi -> Wardrive. Continuous AP discovery + a relative bearing integrated
// from the gyro (no magnetometer — drifts; A-hold zeros it). RAM log,
// CSV export over serial (real GPS via Grove is a later add). A-hold =
// zero bearing, B = export CSV, B-hold = back.
class WardriveScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  struct Entry {
    uint8_t bssid[6];
    std::string ssid;
    int8_t rssi;
    uint8_t ch;
    uint8_t enc;
    float bearing;
    uint32_t ms;
  };
  std::vector<Entry> log_;
  int sel_ = 0;
  float bearing_ = 0;
  uint32_t lastImu_ = 0;
  bool scanning_ = false;
  uint32_t nextScanMs_ = 0;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void exportCsv();
  void draw();
};
