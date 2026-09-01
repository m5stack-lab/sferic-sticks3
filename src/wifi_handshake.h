#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stream.h"  // StrikeEmit
#include "ui.h"

// Headless streaming session (ADR-0003 §6): passive EAPOL capture. Emits an
// "eapol" strike per captured EAPOL frame and a "handshake" strike the first
// time a BSSID reaches 4 EAPOL frames (likely-complete 4-way). Frames still
// land in the shared pcap buffer. See src/stream.cpp.
bool wifiHandshakeStreamStart(const std::string& params);
void wifiHandshakeStreamPoll(const StrikeEmit& emit);
void wifiHandshakeStreamStop();

// WiFi -> Handshake Capture. Passive. Hops channels, writes every beacon
// (once per BSSID, for the SSID + RSN) and every EAPOL frame into the shared
// pcap buffer. Per-BSSID EAPOL count; a BSSID with >= 2 EAPOL frames is
// flagged as a likely-complete handshake. No deauth (ADR-0001 scope).
// A = scroll, B = export pcap over serial, B-hold = back.
class WifiHandshakeScreen : public Screen {
 public:
  void onEnter() override;
  void onExit() override;
  void tick() override;

 private:
  struct Ap {
    uint8_t bssid[6];
    std::string ssid;
    uint8_t eapol;
    int8_t rssi;
  };
  std::vector<Ap> aps_;
  int top_ = 0;
  int ch_ = 1;
  uint32_t lastHop_ = 0;
  uint32_t lastDraw_ = 0;
  bool dirty_ = true;
  void draw();
};
