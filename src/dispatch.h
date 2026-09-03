#pragma once

#include <ArduinoJson.h>

#include "link.h"

// Transport-neutral command dispatch (ADR-0009 §5). Every op the serial and
// WebSocket links accept is handled here; the link only does framing.
//
// `cmd` is the parsed command object: { "id":<int>, "op":"<name>",
// "tool"?:"<id>", "params"?:{...}, "token"?:"<hex>", "enabled"?:<bool> }.
// Replies and errors are written to `link.sendReply(...)`.
namespace dispatch {

void handle(JsonObjectConst cmd, Link& link);

// Fill a station capability block: { id, model, fw_version, toolsets[],
// radios[] }. Used by the `hello` reply (serial) and the `hello` frame the
// WS station sends on connect (ADR-0009 §2).
void stationInfo(JsonObject station);

}  // namespace dispatch
