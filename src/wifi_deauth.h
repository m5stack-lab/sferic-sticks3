#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stream_hub.h"  // StrikeEmit
#include "ui.h"

// Headless streaming session (ADR-0003 §6): passive promiscuous deauth watch,
// emits a "deauth" strike when a BSSID's 1s deauth rate first crosses the
// attack threshold (re-armed once it calms). See src/stream.cpp.
bool wifiDeauthStreamStart(const std::string& params);
void wifiDeauthStreamPoll(const StrikeEmit& emit);
void wifiDeauthStreamStop();

// WiFi -> Deauth Detector. Passive. Counts 802.11 deauth (0x0C) and
// disassoc (0x0A) management frames per BSSID and their rate; flags an
// attack when a BSSID's deauth rate spikes. Detect-only. A = scroll,
// A-hold = clear, B-hold = back.
class WifiDeauthScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  struct Ap {
    uint8_t bssid[6];
    uint32_t deauth;
    uint32_t disassoc;
    uint32_t rate;         // deauth/s over the last window
    uint32_t lastCount;    // deauth count at last window
  };
  std::vector<Ap> aps_;
  uint32_t totalD_ = 0;
  uint32_t totalDis_ = 0;
  int ch_ = 1;
  int top_ = 0;
  uint32_t lastHop_ = 0;
  uint32_t lastRate_ = 0;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void draw();
};
