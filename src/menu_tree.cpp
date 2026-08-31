#include "menu_tree.h"

#include <cstdio>
#include <vector>

#include "ble_scan.h"
#include "hal.h"
#include "ir_tool.h"
#include "motion.h"
#include "registry.h"
#include "tools_screen.h"
#include "wifi_monitor.h"
#include "wifi_scan.h"

const char* toolsetLabel(const std::string& t) {
  if (t == "wifi") return "WiFi";
  if (t == "ble") return "Bluetooth";
  if (t == "ir") return "Infrared";
  if (t == "usb") return "USB";
  if (t == "sensors") return "Sensors";
  if (t == "subghz") return "SubGHz";
  if (t == "nrf24") return "2.4GHz";
  return t.c_str();
}

namespace {

std::string aboutText() {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
                "sferic\nStickS3 multitool  MIT\n\n"
                "PSRAM  %u KB\nheap   %u KB\nLCD    %dx%d\nbatt   %d%%\n"
                "power  %s\n\nA=next  A(hold)=prev\nB=select  B(hold)=back",
                (unsigned)(hal.psramBytes() / 1024),
                (unsigned)(hal.freeHeap() / 1024), hal.w(), hal.h(),
                hal.battPercent(), hal.onUsb() ? "USB" : "battery");
  return buf;
}

// A toolset submenu, rebuilt from the registry each time it is opened.
std::vector<MenuItem> buildToolset(const std::string& tsid) {
  std::vector<MenuItem> items;
  for (const auto& t : registry.all()) {
    if (t.toolset != tsid || !registry.enabled(t.id)) continue;
    auto make = t.make;
    items.push_back({t.name, [make] { ui.push(make()); }});
  }
  if (items.empty()) items.push_back({"(all disabled)", [] {}});
  return items;
}

std::vector<MenuItem> buildRoot() {
  std::vector<MenuItem> root;
  for (const auto& ts : registry.toolsets()) {
    bool any = false;
    for (const auto& t : registry.all())
      if (t.toolset == ts && registry.enabled(t.id)) {
        any = true;
        break;
      }
    if (!any) continue;  // hide a fully-disabled toolset

    const std::string label = toolsetLabel(ts);
    const std::string tsid = ts;
    root.push_back({label, [label, tsid] {
                      ui.push(new MenuScreen(
                          label, [tsid] { return buildToolset(tsid); }));
                    }});
  }
  root.push_back({"Settings", [] {
                   ui.push(new MenuScreen(
                       "Settings",
                       std::vector<MenuItem>{
                           {"Tools", [] { ui.push(new ToolsScreen()); }},
                           {"About", [] {
                              ui.push(new InfoScreen("About", aboutText()));
                            }},
                       }));
                 }});
  return root;
}

}  // namespace

void registerBuiltinTools() {
  registry.add({"wifi.scan", "AP Scan", "wifi", "Scan 2.4GHz access points",
                [] { return static_cast<Screen*>(new WifiScanScreen()); }});
  registry.add({"ble.scan", "BLE Scan", "ble",
                "List advertising BLE devices",
                [] { return static_cast<Screen*>(new BleScanScreen()); }});
  registry.add({"ir.capture", "Learn / Replay", "ir",
                "Capture and re-send an IR signal",
                [] { return static_cast<Screen*>(new IrScreen()); }});
  registry.add({"wifi.monitor", "Packet Monitor", "wifi",
                "Promiscuous 802.11 frame counts",
                [] { return static_cast<Screen*>(new WifiMonitorScreen()); }});
  registry.add({"sensors.motion", "Motion / Tamper", "sensors",
                "BMI270 movement watch",
                [] { return static_cast<Screen*>(new MotionScreen()); }});
}

Screen* makeRootMenu() { return new MenuScreen("sferic", buildRoot); }
