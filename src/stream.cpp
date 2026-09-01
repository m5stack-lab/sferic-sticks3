#include "stream.h"

#include <Arduino.h>
#include <esp_system.h>

#include <cstdio>
#include <vector>

#include "ble_spam.h"
#include "evil_portal.h"
#include "hal.h"
#include "mic_spectrum.h"
#include "registry.h"
#include "wardrive.h"
#include "wifi_deauth.h"
#include "wifi_handshake.h"
#include "wifi_monitor.h"

namespace {

std::vector<StreamTool> g_tools;
const StreamTool* g_active = nullptr;
String g_flash;
uint32_t g_count = 0;

const StreamTool* find(const std::string& id) {
  for (const auto& t : g_tools)
    if (id == t.id) return &t;
  return nullptr;
}

void writeLine(const JsonDocument& d, char prefix) {
  String body;
  body.reserve(measureJson(d) + 1);
  serializeJson(d, body);
  Serial.write(prefix);
  Serial.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
  Serial.write('\n');
  Serial.flush();
}

// The emit passed into a tool's poll(): wrap `decoded` in the strike envelope
// and push one '!' line.
void emitStrike(const char* kind, const JsonDocument& decoded) {
  JsonDocument d;
  d["v"] = 1;
  d["flash"] = g_flash;
  d["id"] = g_flash + "-" + String(g_count);
  d["station"] = hal.stationId();
  d["ts"] = String("t+") + String(millis());
  d["toolset"] = g_active->toolset;
  d["kind"] = kind;
  d["payload"]["decoded"] = decoded;
  writeLine(d, '!');
  ++g_count;
}

}  // namespace

void stream::registerTools() {
  g_tools.push_back({"wifi.monitor", "wifi", &wifiMonitorStreamStart,
                     &wifiMonitorStreamPoll, &wifiMonitorStreamStop});
  g_tools.push_back({"wifi.deauth", "wifi", &wifiDeauthStreamStart,
                     &wifiDeauthStreamPoll, &wifiDeauthStreamStop});
  g_tools.push_back({"wifi.handshake", "wifi", &wifiHandshakeStreamStart,
                     &wifiHandshakeStreamPoll, &wifiHandshakeStreamStop});
  g_tools.push_back({"ble.spam", "ble", &bleSpamStreamStart,
                     &bleSpamStreamPoll, &bleSpamStreamStop});
  g_tools.push_back({"sensors.mic", "sensors", &micStreamStart,
                     &micStreamPoll, &micStreamStop});
  g_tools.push_back({"wifi.wardrive", "wifi", &wardriveStreamStart,
                     &wardriveStreamPoll, &wardriveStreamStop});
  g_tools.push_back({"wifi.portal", "wifi", &portalStreamStart,
                     &portalStreamPoll, &portalStreamStop});
}

bool stream::begin(const std::string& id, const std::string& params,
                   std::string& flashOut, std::string& err) {
  if (g_active) {
    err = "busy";
    return false;
  }
  const StreamTool* t = find(id);
  if (!t) {
    err = "unknown_tool";
    return false;
  }
  if (!registry.enabled(id)) {
    err = "capability_disabled";
    return false;
  }
  if (!t->start(params)) {
    err = "start_failed";
    return false;
  }
  char f[10];
  std::snprintf(f, sizeof(f), "%06lx", (unsigned long)(esp_random() & 0xFFFFFF));
  g_flash = f;
  g_count = 0;
  g_active = t;
  flashOut = f;

  JsonDocument d;
  d["flash"] = g_flash;
  d["event"] = "open";
  d["tool"] = id;
  writeLine(d, '!');
  return true;
}

void stream::end(uint32_t& countOut) {
  countOut = g_count;
  if (!g_active) return;
  g_active->stop();

  JsonDocument d;
  d["flash"] = g_flash;
  d["event"] = "close";
  d["count"] = g_count;
  writeLine(d, '!');

  g_active = nullptr;
  g_flash = "";
}

void stream::poll() {
  if (g_active) g_active->poll(emitStrike);
}

bool stream::active() { return g_active != nullptr; }
const char* stream::activeId() { return g_active ? g_active->id : ""; }
const char* stream::activeFlash() { return g_flash.c_str(); }
uint32_t stream::count() { return g_count; }
