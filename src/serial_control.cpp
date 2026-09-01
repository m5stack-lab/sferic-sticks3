#include "serial_control.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

#include "hal.h"
#include "pcap.h"
#include "registry.h"
#include "version.h"

SerialControl serialCtl;

namespace {

// stick-<last 6 hex of the eFuse MAC> — stable per device, no NVS needed.
String stationId() {
  const uint64_t mac = ESP.getEfuseMac();
  char b[20];
  std::snprintf(b, sizeof(b), "stick-%06lx", (unsigned long)(mac & 0xFFFFFF));
  return b;
}

void reply(const JsonDocument& d) {
  Serial.print('<');
  serializeJson(d, Serial);
  Serial.print('\n');
}

void replyErr(long id, const char* code) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = false;
  d["error"] = code;
  reply(d);
}

void opHello(long id) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  JsonObject s = d["station"].to<JsonObject>();
  s["id"] = stationId();
  s["model"] = "m5stack-sticks3";
  s["fw_version"] = SFERIC_VERSION " " SFERIC_BUILD;
  JsonArray ts = s["toolsets"].to<JsonArray>();
  for (const auto& t : registry.toolsets()) ts.add(t);
  JsonArray rd = s["radios"].to<JsonArray>();
  rd.add("wifi-2g4");
  rd.add("ble");
  rd.add("ir-tx");
  rd.add("ir-rx");
  reply(d);
}

void opStatus(long id) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  d["uptime_s"] = millis() / 1000;
  d["heap"] = (uint32_t)hal.freeHeap();
  d["batt"] = hal.battPercent();
  d["on_usb"] = hal.onUsb();
  d["awake"] = hal.awake();
  JsonObject c = d["capture"].to<JsonObject>();
  c["frames"] = capture.frames();
  c["used"] = (uint32_t)capture.used();
  c["cap"] = (uint32_t)capture.capacity();
  c["recording"] = capture.recording();
  c["full"] = capture.full();
  reply(d);
}

void opTools(long id) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  JsonArray arr = d["tools"].to<JsonArray>();
  for (const auto& t : registry.all()) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = t.id;
    o["name"] = t.name;
    o["toolset"] = t.toolset;
    o["summary"] = t.summary;
    o["enabled"] = registry.enabled(t.id);
    o["has_run"] = static_cast<bool>(t.run);
  }
  reply(d);
}

void opRun(long id, JsonObjectConst cmd) {
  const char* tool = cmd["tool"] | "";
  if (!*tool) {
    replyErr(id, "missing_tool");
    return;
  }
  String params;
  if (!cmd["params"].isNull()) serializeJson(cmd["params"], params);

  const RunResult r = registry.run(tool, params.c_str());
  if (r.outcome == RunOutcome::Ok) {
    JsonDocument d;
    d["id"] = id;
    d["ok"] = true;
    JsonDocument res;  // the tool returns a JSON string — re-embed it as JSON
    if (deserializeJson(res, r.body) == DeserializationError::Ok)
      d["result"] = res;
    else
      d["result"] = r.body;
    reply(d);
    return;
  }
  const char* code = "error";
  switch (r.outcome) {
    case RunOutcome::NotFound:
      code = "capability_not_present";
      break;
    case RunOutcome::Disabled:
      code = "capability_disabled";
      break;
    case RunOutcome::NoHeadlessPath:
      code = "device_only";
      break;
    default:
      break;
  }
  replyErr(id, code);
}

void opSet(long id, JsonObjectConst cmd) {
  const char* tool = cmd["tool"] | "";
  if (!*tool) {
    replyErr(id, "missing_tool");
    return;
  }
  const bool en = cmd["enabled"] | true;
  registry.setEnabled(tool, en);
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  d["enabled"] = registry.enabled(tool);
  reply(d);
}

}  // namespace

void SerialControl::handleLine() {
  buf_[len_] = '\0';
  JsonDocument d;
  if (deserializeJson(d, buf_) != DeserializationError::Ok) {
    replyErr(0, "bad_json");
    return;
  }
  const long id = d["id"] | 0;
  const char* op = d["op"] | "";
  JsonObjectConst cmd = d.as<JsonObjectConst>();

  if (!std::strcmp(op, "hello"))
    opHello(id);
  else if (!std::strcmp(op, "status"))
    opStatus(id);
  else if (!std::strcmp(op, "tools"))
    opTools(id);
  else if (!std::strcmp(op, "run"))
    opRun(id, cmd);
  else if (!std::strcmp(op, "set"))
    opSet(id, cmd);
  else
    replyErr(id, "unknown_op");
}

void SerialControl::poll() {
  while (Serial.available()) {
    const int ch = Serial.read();

    if (!inCmd_) {
      switch (ch) {
        case '>':
          inCmd_ = true;
          len_ = 0;
          break;
        case 'a':
          hal.injectNav(Btn::A, BtnEvent::Click);
          break;
        case 'A':
          hal.injectNav(Btn::A, BtnEvent::Hold);
          break;
        case 'b':
          hal.injectNav(Btn::B, BtnEvent::Click);
          break;
        case 'B':
          hal.injectNav(Btn::B, BtnEvent::Hold);
          break;
        default:
          break;  // log echo, whitespace, noise — ignore
      }
      continue;
    }

    if (ch == '\n' || ch == '\r') {
      if (len_ > 0) handleLine();
      inCmd_ = false;
      len_ = 0;
      continue;
    }
    if (len_ < kMaxLine - 1) {
      buf_[len_++] = static_cast<char>(ch);
    } else {
      replyErr(0, "line_overflow");
      inCmd_ = false;
      len_ = 0;
    }
  }
}
