#include "dispatch.h"

#include <Arduino.h>

#include <cstring>
#include <string>

#include "emit.h"
#include "hal.h"
#include "pcap.h"
#include "registry.h"
#include "stream_hub.h"
#include "version.h"
#ifdef SFERIC_RF_PACK
#include "rf_pack.h"
#endif

namespace {

void replyErr(Link& link, long id, const char* code) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = false;
  d["error"] = code;
  link.sendReply(d);
}

void opHello(Link& link, long id) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  dispatch::stationInfo(d["station"].to<JsonObject>());
  link.sendReply(d);
}

void opStatus(Link& link, long id) {
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
  link.sendReply(d);
}

void opTools(Link& link, long id) {
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
  link.sendReply(d);
}

void opRun(Link& link, long id, JsonObjectConst cmd) {
  const char* tool = cmd["tool"] | "";
  if (!*tool) {
    replyErr(link, id, "missing_tool");
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
    link.sendReply(d);
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
  replyErr(link, id, code);
}

void opSet(Link& link, long id, JsonObjectConst cmd) {
  const char* tool = cmd["tool"] | "";
  if (!*tool) {
    replyErr(link, id, "missing_tool");
    return;
  }
  const bool en = cmd["enabled"] | true;
  registry.setEnabled(tool, en);
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  d["enabled"] = registry.enabled(tool);
  link.sendReply(d);
}

void opStreamStart(Link& link, long id, JsonObjectConst cmd) {
  const char* tool = cmd["tool"] | "";
  if (!*tool) {
    replyErr(link, id, "missing_tool");
    return;
  }
  String params;
  if (!cmd["params"].isNull()) serializeJson(cmd["params"], params);
  std::string flash, err;
  if (!stream::begin(tool, params.c_str(), link, flash, err)) {
    replyErr(link, id, err.c_str());
    return;
  }
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  d["flash"] = flash;
  link.sendReply(d);
}

void opStreamStop(Link& link, long id) {
  uint32_t n = 0;
  const bool wasActive = stream::active();
  stream::end(n);
  JsonDocument d;
  d["id"] = id;
  d["ok"] = wasActive;
  d["count"] = n;
  if (!wasActive) d["error"] = "not_streaming";
  link.sendReply(d);
}

void opStreamStatus(Link& link, long id) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  d["active"] = stream::active();
  d["tool"] = stream::activeId();
  d["flash"] = stream::activeFlash();
  d["count"] = stream::count();
  link.sendReply(d);
}

void opEmitArm(Link& link, long id, JsonObjectConst cmd) {
  const char* tool = cmd["tool"] | "";
  if (!*tool) {
    replyErr(link, id, "missing_tool");
    return;
  }
  String params;
  if (!cmd["params"].isNull()) serializeJson(cmd["params"], params);
  std::string token, err;
  uint32_t window = 0;
  if (!emit::arm(tool, params.c_str(), token, window, err)) {
    replyErr(link, id, err.c_str());
    return;
  }
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  d["token"] = token;
  d["expires_ms"] = window;
  link.sendReply(d);
}

void opEmitFire(Link& link, long id, JsonObjectConst cmd) {
  const char* token = cmd["token"] | "";
  std::string result, err;
  if (!emit::fire(token, result, err)) {
    replyErr(link, id, err.c_str());
    return;
  }
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  JsonDocument r;
  if (deserializeJson(r, result) == DeserializationError::Ok)
    d["result"] = r;
  else
    d["result"] = result;
  link.sendReply(d);
}

void opEmitStatus(Link& link, long id) {
  JsonDocument d;
  d["id"] = id;
  d["ok"] = true;
  d["armed"] = emit::armed();
  d["tool"] = emit::armedId();
  link.sendReply(d);
}

}  // namespace

void dispatch::stationInfo(JsonObject s) {
  s["id"] = hal.stationId();
  s["mac"] = hal.macAddress();
  s["model"] = "m5stack-sticks3";
  s["fw_version"] = SFERIC_VERSION " " SFERIC_BUILD;
  JsonArray ts = s["toolsets"].to<JsonArray>();
  for (const auto& t : registry.toolsets()) ts.add(t);
  JsonArray rd = s["radios"].to<JsonArray>();
  rd.add("wifi-2g4");
  rd.add("ble");
  rd.add("ir-tx");
  rd.add("ir-rx");
#ifdef SFERIC_RF_PACK
  if (rfpack::presence().cc1101) rd.add("cc1101-subghz");
  if (rfpack::presence().nrf24) rd.add("nrf24-2g4");
#endif
}

void dispatch::handle(JsonObjectConst cmd, Link& link) {
  const long id = cmd["id"] | 0;
  const char* op = cmd["op"] | "";

  if (!std::strcmp(op, "hello"))
    opHello(link, id);
  else if (!std::strcmp(op, "status"))
    opStatus(link, id);
  else if (!std::strcmp(op, "tools"))
    opTools(link, id);
  else if (!std::strcmp(op, "run"))
    opRun(link, id, cmd);
  else if (!std::strcmp(op, "set"))
    opSet(link, id, cmd);
  else if (!std::strcmp(op, "stream_start"))
    opStreamStart(link, id, cmd);
  else if (!std::strcmp(op, "stream_stop"))
    opStreamStop(link, id);
  else if (!std::strcmp(op, "stream_status"))
    opStreamStatus(link, id);
  else if (!std::strcmp(op, "emit_arm"))
    opEmitArm(link, id, cmd);
  else if (!std::strcmp(op, "emit_fire"))
    opEmitFire(link, id, cmd);
  else if (!std::strcmp(op, "emit_status"))
    opEmitStatus(link, id);
  else
    replyErr(link, id, "unknown_op");
}
