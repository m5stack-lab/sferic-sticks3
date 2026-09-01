#pragma once

#include <ArduinoJson.h>

#include <functional>
#include <string>

// Background capture sessions for the streaming tools (ADR-0003 §6). Exactly
// one runs at a time — single radio, single-threaded loop(). While active the
// firmware emits NDJSON strike lines over serial, one per event, prefixed
// '!' (vs '<' for command replies); flash open/close are '!' control lines.
//
//   >{"id":N,"op":"stream_start","tool":"wifi.deauth","params":{}}
//   <{"id":N,"ok":true,"flash":"f1a2b3"}
//   !{"flash":"f1a2b3","event":"open","tool":"wifi.deauth"}
//   !{"v":1,"flash":"f1a2b3","id":"f1a2b3-0","station":"stick-…","ts":"t+12345",
//     "toolset":"wifi","kind":"deauth","payload":{"decoded":{…}}}
//   >{"id":M,"op":"stream_stop"}
//   !{"flash":"f1a2b3","event":"close","count":7}
//   <{"id":M,"ok":true,"count":7}

// Called from a stream tool's poll() to emit one strike. `decoded` becomes
// payload.decoded; the envelope (id/ts/flash/station/toolset) is added here.
using StrikeEmit = std::function<void(const char* kind, const JsonDocument& decoded)>;

struct StreamTool {
  const char* id;
  const char* toolset;
  std::function<bool(const std::string& params)> start;  // false => refused
  std::function<void(const StrikeEmit&)> poll;           // each loop() tick
  std::function<void()> stop;
};

namespace stream {

void registerTools();  // boot

// begin: "" flashOut + err set on failure (busy | unknown_tool |
// capability_disabled | start_failed).
bool begin(const std::string& id, const std::string& params, std::string& flashOut,
           std::string& err);
void end(uint32_t& countOut);
void poll();  // call once per loop(); no-op when idle

bool active();
const char* activeId();
const char* activeFlash();
uint32_t count();

}  // namespace stream
