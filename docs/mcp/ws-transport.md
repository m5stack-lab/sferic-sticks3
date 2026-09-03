# WebSocket transport — station ↔ collector over the network

**Status:** implemented (firmware, milestone 3). Firmware:
`src/ws_station.cpp` (link) + `src/dispatch.cpp` (shared op handlers). Build:
`env:sferic-ws` (`-DSFERIC_WS`), config in `src/ws_config.h` (gitignored;
copy `ws_config.h.example`). Collector side: deferred (a `WebSocketStation`
`StationTransport` + a WS route on the collector's HTTP server).
**Relates to:** ADR-0009 (this decision), ADR-0003 §6, ADR-0007, ADR-0008.

Same op vocabulary as `serial-transport.md` — `hello` / `status` / `tools` /
`run` / `set` / `stream_*` / `emit_*`, same fields, same error codes. Only
the framing differs. The op handlers are one transport-neutral module
(`src/dispatch.*`); each link just frames.

## Connection

The station always dials out — one persistent WebSocket to
`ws://<collector-host>:<port>/station` (`wss://` when `WS_USE_TLS=1`). The
collector never dials the station (stations roam behind NAT; only the Pi has
a stable address — ADR-0008).

1. TCP → HTTP upgrade. The upgrade carries `Authorization: Bearer <token>`
   (`WS_STATION_TOKEN`, provisioned out of band, never committed).
2. On `open` the station sends one `hello` frame (below). The collector
   replies `welcome`.
3. A bad/missing token → the collector closes the socket (code 4001).

**Reconnect:** on any drop the station retries forever (fixed 3 s in this
build; ADR-0009 §3 wants jittered exponential 1 s→30 s — a later refinement,
the library only does a fixed interval). On reconnect it re-sends `hello`;
the collector treats the same `station.id` as that station resuming.

**Liveness:** WS ping/pong every 15 s, 3 s pong timeout, drop after 2 misses
(library heartbeat). An app-level `{"type":"ping"}` is answered with
`{"type":"pong"}` too.

**Session death (ADR-0009 §3):** the instant the socket drops, the firmware
aborts any `stream_*` session (radio released) and clears any pending
`emit` arm. The collector must not assume a session survives a reconnect —
it re-issues `stream_start` if it still wants the capture.

## Framing

Every message is one WebSocket **text frame = one JSON object** with a
`type` discriminator.

| `type` | Direction | Meaning |
|---|---|---|
| `hello` | station → collector | first frame after connect; capability decl + token |
| `welcome` | collector → station | ack; carries `collector` name + wall-clock `time` |
| `cmd` | collector → station | a command — `{id, op, tool?, params?, token?, enabled?}` |
| `reply` | station → collector | correlated reply — `{id, ok, ...}` / `{id, ok:false, error}` |
| `strike` | station → collector | one strike during a stream session (envelope per `strike-format.md`) |
| `event` | station → collector | stream lifecycle — `{flash, event:"open"\|"close", ...}` |
| `ping` / `pong` | either | app-level keepalive on top of WS ping/pong |

`cmd` is exactly a serial command line with `"type":"cmd"` added and the `>`
dropped. `reply` / `strike` / `event` are the `<` / `!` lines with a `type`
key instead of the prefix. `ts` stays boot-relative `"t+<ms>"`; the
collector normalizes to wall-clock on ingest and keeps the original in
`meta.device_ts`.

### `hello`

```jsonc
{ "type":"hello", "token":"<per-station bearer>",
  "station":{ "id":"stick-6e27ac", "mac":"ac:27:6e:d2:5d:0c",
              "model":"m5stack-sticks3", "fw_version":"0.1.0 v0.1.0-…",
              "toolsets":["wifi","ble","ir","sensors"],
              "radios":["wifi-2g4","ble","ir-tx","ir-rx"] } }
```

Same `station` block as the serial `hello` reply (`dispatch::stationInfo`);
`mac` is the Wi-Fi STA MAC, also logged on boot as `[ws] sta mac …`.

### `welcome`

```jsonc
{ "type":"welcome", "collector":"pi-collector", "time":"2026-09-02T18:22:04Z" }
```

`time` lets the station show real wall-clock; it does **not** re-timestamp
strikes (those stay `t+<ms>`).

### `cmd` → `reply`

```jsonc
// collector → station
{ "type":"cmd", "id":42, "op":"run", "tool":"ble.scan", "params":{"ms":4000} }
// station → collector
{ "type":"reply", "id":42, "ok":true, "result":{ "devices":[ … ] } }
```

### stream session

```jsonc
{ "type":"cmd", "id":7, "op":"stream_start", "tool":"wifi.monitor", "params":{} }
{ "type":"reply", "id":7, "ok":true, "flash":"cf5e10" }
{ "type":"event",  "flash":"cf5e10", "event":"open", "tool":"wifi.monitor" }
{ "type":"strike", "v":1, "flash":"cf5e10", "id":"cf5e10-0", "station":"stick-6e27ac",
  "ts":"t+13886", "toolset":"wifi", "kind":"monitor",
  "payload":{ "decoded":{ "total":92, "rate":83, "rssi":-59, … } } }
{ "type":"cmd", "id":8, "op":"stream_stop" }
{ "type":"event", "flash":"cf5e10", "event":"close", "count":10 }
{ "type":"reply", "id":8, "ok":true, "count":10 }
```

The strike/event frames go back out **the link the `stream_start` arrived
on** — if the collector started it over WS, they come over WS; a serial
`stream_start` still emits `!` lines on serial. Both links can be present in
one `sferic-ws` build.

## Wi-Fi radio contention (StickS3)

The StickS3 has one 2.4 GHz radio. It carries the uplink **and** is what the
`wifi.*` tools need. Running a `wifi.*` stream (`wifi.monitor` /
`wifi.deauth` / `wifi.handshake` / `wifi.wardrive` / `wifi.portal`) puts the
radio into promiscuous or AP mode and **drops the WebSocket** for the
duration; the station reconnects when the capture ends (and the collector
gets the `close` event only if the socket outlived the tool — otherwise it
sees a disconnect and should treat the session as ended).

`ble.*`, `ir.*`, `sensors.*`, and the RF-Pack `subghz.*` / `nrf24.*` tools
do not touch the Wi-Fi radio and stream fine over the live uplink. For
`wifi.*` captures over the network, prefer the serial transport, or accept
the reconnect. A co-processor radio (the Tab5's ESP32-C6) removes this
constraint on that class of station.

## Backpressure & offline (ADR-0009 §4)

- One active `stream_*` per station, enforced firmware-side.
- If strikes outpace the link the station drops oldest and (planned) reports
  the count in the next strike's `meta.dropped`. It never blocks `loop()`.
  *This build: strikes emitted while `!isConnected()` are dropped silently —
  the bounded-queue + `meta.dropped` counter is a follow-up.*
- No collector reachable → the station runs fully standalone (menu, local
  Web UI). It has no durable store (no SD), so strikes generated while
  disconnected are lost. The collector is the system of record.

## Not yet

- Collector-side `WebSocketStation` transport + WS route (deferred with the
  Pi collector phase).
- Jittered exponential reconnect backoff.
- Bounded outbound strike queue with `meta.dropped`.
- pcap buffer pull as a framed op (still the base64 `Settings > Capture`
  export).
