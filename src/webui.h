#pragma once

#include <cstdint>

#include "ui.h"

// Settings > Web UI (ADR-0006). Starts a SoftAP + HTTP server; the exclusive
// WiFi user while it is up. Serves a one-page config/monitor UI and a pcap
// download. No tool logic — every mutation goes through Registry. B-hold =
// stop and back.
class WebUiScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
  void draw();
};
