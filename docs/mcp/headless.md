# Headless `run()` — the shared launch path

**Status:** seam landed (firmware), MCP side deferred (ADR-0003 §8, ADR-0004 §4).
**Firmware:** `src/tool.h` (`Tool::run`), `src/registry.cpp` (`Registry::run`),
`src/webui.cpp` (`/api/tools/run`).

A tool is no longer only a `Screen`. Every `Tool` may also carry:

```cpp
std::function<std::string(const std::string& params)> run;  // nullable
```

`params` is an opaque string for now (a query-string value; JSON later). The
tool returns a JSON object as a string. `run` is **null** when the tool has no
headless path yet — it is then device-menu only.

## One dispatch, three callers

The device menu, the web UI, and (later) the MCP server all go through
`Registry::run(id, params)`, which enforces the same two gates in one place:

```cpp
RunResult Registry::run(id, params) const;

enum class RunOutcome { Ok, NotFound, Disabled, NoHeadlessPath };
```

| Outcome | Meaning | MCP error (ADR-0003 §3) | HTTP (`/api/tools/run`) |
|---|---|---|---|
| `Ok` | ran; `body` is the tool's JSON | — | `200` |
| `NotFound` | no such tool id | `capability_not_present` | `404` |
| `Disabled` | registered but toggled off | `capability_disabled` | `409` |
| `NoHeadlessPath` | registered, enabled, but `run` is null (device-only) | `device_only` | `501` |

## Web route

```
GET /api/tools/run?id=<tool-id>[&p=<params>]
```

Success envelope:

```json
{ "id": "sensors.motion", "ok": true, "result": { "ax": 0.01, "ay": 0.02, "az": 0.99,
  "accel_dev_g": 0.004, "gyro_dps": 1.2, "moving": false, "face": "flat" } }
```

Failure:

```json
{ "id": "wifi.scan", "ok": false, "error": "device_only" }
```

`GET /api/tools` now also reports `"has_run": true|false` per tool so a client
knows what it can launch.

## What exposes `run` today

| Tool | `run` result | Notes |
|---|---|---|
| `sensors.motion` / `sensors.gesture` | one-shot IMU snapshot (`imuSnapshot()`) | pure I2C, no radio — safe even while the Web UI SoftAP is up |
| `wifi.scan` | `{count, aps:[{ssid,bssid,rssi,channel,enc}]}` | blocking async scan, `{"ms":…}` param (500–15000, def 4000); `{"error":"radio_busy"}` if the SoftAP is up |
| `ble.scan` | `{count, devices:[{addr,addr_type,name,rssi,connectable,services}]}` | blocking scan, `{"ms":…}`; BLE + SoftAP coexist so no radio guard |
| `wifi.probe` | `{total, uniq, probes:[{mac,ssid,rssi,count}]}` | channel-hops for `{"ms":…}` (1000–20000, def 4000), blocking; `radio_busy` guard |
| `ir.decode` | `{protocol,value,bits}` \| `{protocol:"RAW",samples}` \| `{error:"timeout"}` | listens up to `{"ms":…}` (200–15000, def 3000), blocking; GPIO RX, no radio |
| `ir.capture` | learn one-shot — `{protocol,value,bits,raw:[…]}` | GPIO RX, non-emitting |

**These `run()`s block `loop()` for their duration** (the UI freezes, the
serial channel pauses) — acceptable for one-shot scans over the collector
link.

## Streaming tools — `stream_start` / `stream_stop`

`run()` is one-shot. The long-running tools instead have a background
session (`src/stream.cpp`): one at a time, emitting `!`-prefixed NDJSON
strikes until stopped. Wired: `wifi.monitor`, `wifi.deauth`,
`wifi.handshake`, `wifi.wardrive`, `wifi.portal`, `ble.spam`, `sensors.mic`.
See `docs/mcp/serial-transport.md`.

## Emitting tools — `emit_arm` / `emit_fire`

`ir.tvbgone`, `ir.replay`, `ble.hid`, `usb.hid` are never a one-call fire
(ADR-0008 §1). `emit_arm` returns a token + a 60 s window; `emit_fire` with
that token runs the action. `src/emit.cpp`.

Every station capability now has a headless path: one-shot `run`, a
`stream_*` session, or `emit_arm`/`emit_fire`.
