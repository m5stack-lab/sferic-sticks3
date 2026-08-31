#include "menu_tree.h"

#include <cstdio>

#include "hal.h"
#include "wifi_scan.h"

namespace {

// Push a "not implemented yet" screen for a leaf tool.
MenuItem stub(const char* label) {
  std::string name = label;
  return {name, [name] {
            ui.push(new InfoScreen(name, name + "\n\nNot implemented yet.\n\n"
                                               "Tracked in ADR-0001."));
          }};
}

MenuItem submenu(const char* label, std::vector<MenuItem> items) {
  std::string name = label;
  return {name, [name, items] { ui.push(new MenuScreen(name, items)); }};
}

std::string aboutText() {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "sferic\nStickS3 multitool  MIT\n\n"
                "PSRAM  %u KB\nheap   %u KB\nLCD    %dx%d\nbatt   %d%%\n"
                "power  %s\n\nA=next A(hold)=prev\nB=select B(hold)=back",
                (unsigned)(hal.psramBytes() / 1024),
                (unsigned)(hal.freeHeap() / 1024), hal.w(), hal.h(),
                hal.battPercent(), hal.onUsb() ? "USB" : "battery");
  return buf;
}

}  // namespace

Screen* makeRootMenu() {
  return new MenuScreen(
      "sferic",
      {
          submenu("WiFi",
                  {
                      {"AP Scan", [] { ui.push(new WifiScanScreen()); }},
                      stub("Packet Monitor"),
                      stub("Probe / Beacon"),
                      stub("Handshake Capture"),
                      stub("Evil Portal"),
                      stub("Wardrive Log"),
                  }),
          submenu("Bluetooth",
                  {
                      stub("BLE Scan"),
                      stub("Advertisement Decode"),
                      stub("Spam Detect"),
                      stub("BLE HID"),
                  }),
          submenu("Infrared",
                  {
                      stub("Learn / Replay"),
                      stub("Protocol Decode"),
                      stub("TV-B-Gone"),
                  }),
          submenu("USB", {stub("Keystroke Injection")}),
          submenu("Sensors",
                  {
                      stub("Motion / Tamper"),
                      stub("Gesture Nav"),
                      stub("Mic Spectrum"),
                  }),
          submenu("RF Pack",
                  {
                      stub("SubGHz Scan"),
                      stub("SubGHz Capture / Replay"),
                      stub("433 Sensor Decode"),
                      stub("2.4GHz Band Scan"),
                      stub("nRF24 Sniffer"),
                  }),
          submenu("Settings",
                  {
                      stub("Backlight"),
                      {"About", [] { ui.push(new InfoScreen("About", aboutText())); }},
                  }),
      });
}
