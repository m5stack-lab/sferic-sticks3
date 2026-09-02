# ADR-0009 — Station ↔ collector transport: WebSocket

**Status:** Accepted (choice + framing). **Implementation deferred** to the
Pi collector phase — the serial transport (`docs/mcp/serial-transport.md`)
covers the tethered dev desk today.
**Date:** 2026-09-01
**Deciders:** ry-ops (operator)
**Relates to:** ADR-0003 (companion MCP server — §6 left the wire format
open), ADR-0007 (fleet composition), ADR-0008 (network uplink)

## Context

ADR-0003 §6 fixed the *roles* of the station ↔ collector link — the station
is the client, the link is persistent — but left the wire format "decide at
implementation", naming WebSocket, MQTT, and a serial/USB link as candidates.

Milestone 2 built the **serial** transport: a line protocol over USB CDC
(`>` command / `<` reply / `!` NDJSON strike stream) with a settled op
vocabulary — `hello`, `status`, `tools`, `run`, `set`, `stream_start` /
`stream_stop` / `stream_status`, `emit_arm` / `emit_fire` / `emit_status`.
The collector reaches it through `SerialStation`, one implementation of a
`StationTransport` ABC.

Milestone 3 is the same station driven over the network — a StickS3 on WiFi
talking to the Pi 5. This ADR picks that wire format and its framing so the
firmware's op handlers and the collector's transport layer can be built
against a fixed target.

## Decision

### 1. WebSocket, station-initiated

The station opens **one persistent WebSocket** to the collector
(`ws://<collector>:<port>/station`, or `wss://` off-tailnet — see §6). The
station always dials out; the collector never dials the station. This keeps
stations behind home NAT working with zero port-forwarding — only the Pi
needs a stable address (its tailnet IP, ADR-0008).

One connection per station. The collector keys it by the `hello.id` and
tracks it in the same `Fleet` the serial and simulated transports use.

### 2. The op vocabulary carries over unchanged

Every serial op keeps its name, fields, and semantics. Only the framing
changes: instead of `>` / `<` / `!` line prefixes, each WebSocket **text
frame is one JSON object** with a `type` discriminator.

```jsonc
// station → collector, first frame after connect (capability decl, ADR-0003 §3)
{ "type":"hello", "token":"<per-station>",
  "station":{ "id":"stick-6e27ac", "model":"m5stack-sticks3",
              "fw_version":"…", "toolsets":["wifi","ble","ir","sensors"],
              "radios":["wifi-2g4","ble","ir-tx","ir-rx"] } }

// collector → station, ack
{ "type":"welcome", "collector":"pi-collector", "time":"2026-09-01T…Z" }

// collector → station, a command  (op/id/params exactly as serial)
{ "type":"cmd", "id":42, "op":"run", "tool":"wifi.scan", "params":{"ms":4000} }

// station → collector, the correlated reply
{ "type":"reply", "id":42, "ok":true, "result":{ … } }

// station → collector, async during a stream session
{ "type":"strike", "flash":"cf5e10", "kind":"monitor",
  "ts":"t+13886", "payload":{ "decoded":{ … } } }
{ "type":"event",  "flash":"cf5e10", "event":"open"|"close", "count":10 }

// either direction, app-level keepalive (on top of WS ping/pong)
{ "type":"ping" }  /  { "type":"pong" }
```

`ts` stays boot-relative `"t+<ms>"`; the collector normalizes to wall-clock
on ingest and keeps the original in `meta.device_ts`, exactly as it does for
the serial stream today. (`welcome.time` lets a station display real time,
not seed strike timestamps.)

### 3. Connection lifecycle

- **Connect:** TCP → HTTP upgrade → station sends `hello` → collector replies
  `welcome`. A `hello` with a bad/missing `token` is closed with a 4001
  code.
- **Reconnect:** on drop, the station retries with capped exponential
  backoff (1 s → 30 s, jittered) forever. On reconnect it re-sends `hello`;
  the collector treats it as the same station (same `id`) resuming.
- **Session death:** any `stream_*` session is dead the instant the socket
  drops — the radio was released on the firmware side. The collector must
  not assume a session survives a reconnect; it re-issues `stream_start` if
  it still wants the capture. An armed `emit` likewise clears.
- **Liveness:** WS ping/pong every 15 s; three missed pongs → the side that
  noticed closes and (station side) reconnects.

### 4. One session, backpressure, offline

- **One active `stream_*` per station**, enforced firmware-side as now
  (single radio, single loop). The collector serializes its own requests per
  station.
- **Backpressure:** the station holds a small bounded outbound queue. If a
  stream produces strikes faster than the link drains, it **drops oldest**
  and reports the count in the next strike's `meta.dropped` — it never
  blocks `loop()`.
- **Offline:** with no collector reachable the station runs fully standalone
  (menu, local web UI). It has no durable store (no SD), so strikes
  generated while disconnected are lost — the collector is the system of
  record (ADR-0007). A future SD-equipped or Tab5-class station may spool;
  the StickS3 does not.

### 5. Serial is not replaced

`SerialStation` stays — the tethered dev-desk path and a fallback when there
is no network. Because both transports speak the same op vocabulary, the
firmware's op handlers factor into one transport-neutral dispatcher with a
thin framing layer per link, and the collector's `StationTransport`
implementations stay interchangeable behind the ABC.

### 6. Security / reachability defers to ADR-0008

On the tailnet or inside a WireGuard tunnel the link is already encrypted —
plain `ws://` is fine. On an untrusted segment, `wss://` with the
collector's certificate. Either way the connect carries a **per-station
bearer token** (upgrade header `Authorization: Bearer …`, or `hello.token`),
provisioned out of band, never committed (`*.example` only). This ADR
assumes a reachable collector endpoint; how the packets get there is
ADR-0008.

## Consequences

- **Firmware (deferred):** a `WsStation` link layer (an Arduino WS client,
  e.g. `links2004/arduinoWebSockets`) + refactor `serial_control.cpp`'s op
  handlers into a `Dispatcher` that a link feeds framed messages. Add a
  `ws` build config with the collector URL + token as `*.example`.
- **Collector (deferred):** a `WebSocketStation` transport and a WS endpoint
  on the collector's HTTP server (it already needs one for the dashboard,
  ADR-0007 §3). No broker, no new daemon.
- `docs/mcp/ws-transport.md` gets written alongside the implementation —
  a living spec like `serial-transport.md`, sharing the op reference.
- The op vocabulary is now effectively frozen across two transports; adding
  an op is a deliberate change to both docs.

## Alternatives considered

- **MQTT.** Rejected as the primary link. It needs a broker running and
  secured on the Pi (a third moving part), plus topic design and QoS
  semantics, to serve a shape that is just N stations → 1 collector.
  Pub/sub fan-out — MQTT's real strength — is not what this link is. MQTT
  could still be added later as an *alternate* `StationTransport` for a
  deployment that already runs a broker.
- **Raw TCP + length-prefixed JSON.** Rejected: reinvents framing, the
  HTTP-upgrade handshake, and proxy/tailnet traversal that WebSocket gives
  for free, for no gain on an MCU.
- **gRPC / protobuf.** Rejected: code-gen and HTTP/2 footprint are heavy for
  an ESP32, and the payloads are already small hand-rolled JSON shared with
  the serial link and the web UI.
- **HTTP long-poll / SSE.** Rejected: no clean station→collector *and*
  collector→station full-duplex; the `!` strike stream and the command
  channel want one bidirectional socket, which is exactly WebSocket.
- **Collector dials the station.** Rejected (re-affirming ADR-0003 §6):
  stations roam behind NAT; only the Pi has a stable address.
