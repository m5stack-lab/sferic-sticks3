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
| everything else | `null` | emits and must stay behind arm/confirm (`ir.tvbgone`, `ir.capture` replay, `*.hid`), or is genuinely streaming (`wifi.monitor`, `wifi.handshake`, `wifi.deauth`, `ble.spam`, `sensors.mic`) and wants a start/stop + strike-stream shape, not one-shot |

**These `run()`s block `loop()` for their duration** (the UI freezes, the
serial channel pauses) — acceptable for one-shot scans over the collector
link; a job/async model comes with the streaming tools.

Emitting tools stay gated regardless of surface — headless or not, firing
needs an explicit confirm (ADR-0008 §1).

## Streaming

`run()` is one-shot request/response. Genuinely long-running tools (handshake
capture, packet monitor, a motion watch) run as a `Screen` on-device or,
later, as an MCP job that streams a **flash** of strikes (ADR-0003 §6).
`/api/tools/run` and the serial `run` op are not streaming endpoints.
