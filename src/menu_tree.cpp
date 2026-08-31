#include "menu_tree.h"

#include <cstdio>
#include <vector>

#include "ble_scan.h"
#include "ble_hid.h"
#include "ble_spam.h"
#include "capture_screen.h"
#include "display_screen.h"
#include "evil_portal.h"
#include "gesture.h"
#include "hal.h"
#include "ir_decode.h"
#include "ir_tool.h"
#include "ir_tvbgone.h"
#include "mic_spectrum.h"
#include "usb_hid.h"
#include "wardrive.h"
#include "webui.h"
#include "wifi_deauth.h"
#include "wifi_handshake.h"
#include "motion.h"
#include "registry.h"
#include "tools_screen.h"
#include "wifi_monitor.h"
#include "wifi_probe.h"
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
                           {"Display", [] { ui.push(new DisplayScreen()); }},
                           {"Capture", [] { ui.push(new CaptureScreen()); }},
                           {"Web UI", [] { ui.push(new WebUiScreen()); }},
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
  registry.add({"ir.tvbgone", "TV-B-Gone", "ir",
                "Sweep common TV power codes",
                [] { return static_cast<Screen*>(new TvBGoneScreen()); }});
  registry.add({"wifi.probe", "Probe / Beacon", "wifi",
                "Collect probe requests (who wants what SSID)",
                [] { return static_cast<Screen*>(new WifiProbeScreen()); }});
  registry.add({"ble.spam", "Spam Detect", "ble",
                "Flag BLE advertising floods",
                [] { return static_cast<Screen*>(new BleSpamScreen()); }});
  registry.add({"sensors.mic", "Mic Spectrum", "sensors",
                "Live FFT of the ES8311 mic",
                [] { return static_cast<Screen*>(new MicSpectrumScreen()); }});
  registry.add({"wifi.portal", "Evil Portal", "wifi",
                "Open AP + captive login capture (authorized testing)",
                [] { return static_cast<Screen*>(new EvilPortalScreen()); }});
  registry.add({"sensors.gesture", "Gesture", "sensors",
                "BMI270 orientation + shake",
                [] { return static_cast<Screen*>(new GestureScreen()); }});
  registry.add({"wifi.handshake", "Handshake Capture", "wifi",
                "Passive EAPOL capture to pcap",
                [] { return static_cast<Screen*>(new WifiHandshakeScreen()); }});
  registry.add({"ir.decode", "Protocol Decode", "ir",
                "Continuous IR decode (peek, no store)",
                [] { return static_cast<Screen*>(new IrDecodeScreen()); }});
  registry.add({"ble.hid", "HID Keyboard", "ble",
                "Advertise as a BLE keyboard, type a payload",
                [] { return static_cast<Screen*>(new BleHidScreen()); }});
  registry.add({"wifi.wardrive", "Wardrive", "wifi",
                "AP log + gyro bearing, CSV export",
                [] { return static_cast<Screen*>(new WardriveScreen()); }});
  registry.add({"wifi.deauth", "Deauth Detector", "wifi",
                "Passive deauth/disassoc watch",
                [] { return static_cast<Screen*>(new WifiDeauthScreen()); }});
#ifdef SFERIC_USB_HID
  registry.add({"usb.hid", "HID Keyboard", "usb",
                "USB keystroke injection (ADR-0005 build)",
                [] { return static_cast<Screen*>(new UsbHidScreen()); }});
#endif
}

Screen* makeRootMenu() { return new MenuScreen("sferic", buildRoot); }
