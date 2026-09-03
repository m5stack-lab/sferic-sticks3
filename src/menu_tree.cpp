#include "menu_tree.h"

#include <ArduinoJson.h>
#include <M5Unified.h>
#include <math.h>

#include <cstdint>
#include <cstdio>
#include <string>
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
#ifdef SFERIC_RF_PACK
#include "nrf24.h"
#include "subghz.h"
#endif
#include "registry.h"
#include "tools_screen.h"
#include "wifi_monitor.h"
#include "wifi_probe.h"
#include "wifi_scan.h"

namespace {

// One-shot IMU read, JSON. Backs the headless run() for the sensor tools:
// pure I2C, no radio, so it works while the Web UI SoftAP is up.
std::string imuSnapshot() {
  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  M5.Imu.getAccel(&ax, &ay, &az);
  M5.Imu.getGyro(&gx, &gy, &gz);
  const float aDev = fabsf(sqrtf(ax * ax + ay * ay + az * az) - 1.0f);
  const float gMag = sqrtf(gx * gx + gy * gy + gz * gz);
  const bool moving = aDev > 0.16f || gMag > 40.0f;  // "med" thresholds
  const char* face = "flat";
  if (az < -0.6f) face = "inverted";
  else if (ax > 0.6f) face = "right";
  else if (ax < -0.6f) face = "left";
  else if (ay > 0.6f) face = "top";
  else if (ay < -0.6f) face = "bottom";
  char buf[192];
  std::snprintf(buf, sizeof(buf),
                "{\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,\"accel_dev_g\":%.3f,"
                "\"gyro_dps\":%.1f,\"moving\":%s,\"face\":\"%s\"}",
                ax, ay, az, aDev, gMag, moving ? "true" : "false", face);
  return buf;
}

// Pull an optional {"ms": <int>} out of a headless params string, clamped.
uint32_t paramMs(const std::string& p, uint32_t def, uint32_t lo, uint32_t hi) {
  JsonDocument d;
  if (deserializeJson(d, p) != DeserializationError::Ok) return def;
  if (!d["ms"].is<uint32_t>()) return def;
  uint32_t ms = d["ms"];
  return ms < lo ? lo : (ms > hi ? hi : ms);
}

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
    items.push_back({t.name, [make] { ui.push(make()); }, tsid});
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
    root.push_back({label,
                    [label, tsid] {
                      ui.push(new MenuScreen(
                          label, [tsid] { return buildToolset(tsid); }));
                    },
                    tsid});
  }
  root.push_back({"Settings",
                  [] {
                    ui.push(new MenuScreen(
                        "Settings",
                        std::vector<MenuItem>{
                            {"Tools", [] { ui.push(new ToolsScreen()); }, "gear"},
                            {"Display", [] { ui.push(new DisplayScreen()); }, "gear"},
                            {"Capture", [] { ui.push(new CaptureScreen()); }, "save"},
                            {"Web UI", [] { ui.push(new WebUiScreen()); }, "globe"},
                            {"About",
                             [] { ui.push(new InfoScreen("About", aboutText())); },
                             "info"},
                        }));
                  },
                  "gear"});
  return root;
}

}  // namespace

void registerBuiltinTools() {
  registry.add({"wifi.scan", "AP Scan", "wifi", "Scan 2.4GHz access points",
                [] { return static_cast<Screen*>(new WifiScanScreen()); },
                [](const std::string& p) {
                  return wifiScanJson(paramMs(p, 4000, 500, 15000));
                }});
  registry.add({"ble.scan", "BLE Scan", "ble",
                "List advertising BLE devices",
                [] { return static_cast<Screen*>(new BleScanScreen()); },
                [](const std::string& p) {
                  return bleScanJson(paramMs(p, 4000, 500, 15000));
                }});
  registry.add({"ir.capture", "Learn / Replay", "ir",
                "Capture and re-send an IR signal",
                [] { return static_cast<Screen*>(new IrScreen()); },
                [](const std::string& p) {
                  return irCaptureLearn(p.empty() ? "{\"ms\":5000}" : p);
                }});
  registry.add({"wifi.monitor", "Packet Monitor", "wifi",
                "Promiscuous 802.11 frame counts",
                [] { return static_cast<Screen*>(new WifiMonitorScreen()); }});
  registry.add({"sensors.motion", "Motion / Tamper", "sensors",
                "BMI270 movement watch",
                [] { return static_cast<Screen*>(new MotionScreen()); },
                [](const std::string&) { return imuSnapshot(); }});
  registry.add({"ir.tvbgone", "TV-B-Gone", "ir",
                "Sweep common TV power codes",
                [] { return static_cast<Screen*>(new TvBGoneScreen()); }});
  registry.add({"wifi.probe", "Probe / Beacon", "wifi",
                "Collect probe requests (who wants what SSID)",
                [] { return static_cast<Screen*>(new WifiProbeScreen()); },
                [](const std::string& p) {
                  return wifiProbeJson(paramMs(p, 4000, 1000, 20000));
                }});
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
                [] { return static_cast<Screen*>(new GestureScreen()); },
                [](const std::string&) { return imuSnapshot(); }});
  registry.add({"wifi.handshake", "Handshake Capture", "wifi",
                "Passive EAPOL capture to pcap",
                [] { return static_cast<Screen*>(new WifiHandshakeScreen()); }});
  registry.add({"ir.decode", "Protocol Decode", "ir",
                "Continuous IR decode (peek, no store)",
                [] { return static_cast<Screen*>(new IrDecodeScreen()); },
                [](const std::string& p) {
                  return irDecodeJson(paramMs(p, 3000, 200, 15000));
                }});
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
#ifdef SFERIC_RF_PACK
  registry.add({"subghz.scan", "Band Scan", "subghz",
                "433 MHz occupancy sweep (CC1101 RSSI)",
                [] { return static_cast<Screen*>(new SubGhzScanScreen()); },
                [](const std::string& p) { return subghzScanJson(p); }});
  registry.add({"nrf24.scan", "Band Scan", "nrf24",
                "2.4 GHz carrier-detect sweep (nRF24 RPD)",
                [] { return static_cast<Screen*>(new Nrf24ScanScreen()); },
                [](const std::string& p) { return nrf24ScanJson(p); }});
#endif
}

Screen* makeRootMenu() { return new MenuScreen("sferic", buildRoot); }
