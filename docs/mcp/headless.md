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

| Tool | `run` | Why |
|---|---|---|
| `sensors.motion` | ✅ one-shot IMU read (`imuSnapshot()`) | pure I2C, no radio — safe while the Web UI SoftAP is up |
| `sensors.gesture` | ✅ same snapshot (orientation `face` + accel/gyro) | same |
| everything else | ✅ null | needs a radio the SoftAP is using (`wifi.*`, `ble.*`), or emits and must stay behind the on-device arm/confirm guard (`ir.tvbgone`, `ir.capture`, `*.hid`), or has no non-interactive shape yet |

Radio tools get their headless path on the **MCP collector** (ADR-0007): the
station links to the Pi over its station radio, so the SoftAP conflict that
blocks them from the local web UI doesn't apply there. Emitting tools stay
gated — headless or not, they need an explicit operator confirm.

## Streaming

`run()` is one-shot request/response. Long-running tools (a handshake capture,
a motion watch) still run as a `Screen` on-device or, later, as an MCP job that
streams a **flash** of strikes (ADR-0003 §6). `/api/tools/run` is not a
streaming endpoint.
