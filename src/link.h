#pragma once

#include <ArduinoJson.h>

// One station <-> collector transport. The op vocabulary (hello / status /
// tools / run / set / stream_* / emit_*) is transport-neutral — see
// src/dispatch.* — and each link only frames the messages its own way:
//
//   SerialLink : '<' reply lines, '!' strike/event lines over USB CDC
//   WsStation  : {"type":"reply"|"strike"|"event", ...} JSON text frames
//
// ADR-0009 §5. A link is passed by reference into the dispatcher and into
// stream::begin(), so a stream session's strikes go back out the same link
// the stream_start arrived on.
struct Link {
  virtual ~Link() = default;

  // A correlated reply to a command. `d` already carries {id, ok, ...}.
  virtual void sendReply(JsonDocument& d) = 0;

  // Async during a stream session. `d` is the full strike envelope
  // (v/flash/id/station/ts/toolset/kind/payload) per docs/mcp/strike-format.md.
  virtual void sendStrike(JsonDocument& d) = 0;

  // Stream lifecycle: `d` carries {flash, event:"open"|"close", ...}.
  virtual void sendEvent(JsonDocument& d) = 0;
};
