#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stream.h"  // StrikeEmit
#include "ui.h"

// WiFi -> Evil Portal. Open SoftAP + catch-all DNS + a generic captive
// login page; captured submissions are logged to serial + a small RAM list
// and NEVER forwarded anywhere (ADR-0001: authorized phishing-resistance
// testing only). A = cycle SSID preset, B = view captures, B-hold = back.
class EvilPortalScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  int ssidIdx_ = 0;
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  void draw();
};

// Shared capture log (also read by the capture-view screen).
struct PortalHit {
  std::string field1;  // e.g. email / user
  std::string field2;  // e.g. password
  uint32_t ms;
};
extern std::vector<PortalHit> g_portalHits;

// Headless streaming (ADR-0003 §6): stand up the open SoftAP + captive login
// page (params: {"ssid": "..."}), emit a "credential" strike per capture.
bool portalStreamStart(const std::string& params);
void portalStreamPoll(const StrikeEmit& emit);
void portalStreamStop();
