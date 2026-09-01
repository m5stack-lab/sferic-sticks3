# Serial transport — station ↔ collector over USB

**Status:** implemented (milestone 2). Firmware: `src/serial_control.cpp`.
**Relates to:** ADR-0003 §6 (station↔server link), ADR-0008 §1 (MCP = full
control plane).

The first real `StationTransport` for `sferic-collector`: a tethered station
on USB. Over serial the 2.4 GHz radio is idle (no SoftAP, no STA), so every
toolset is reachable — unlike the on-device Web UI, which owns the radio
while it is up.

Milestone 3 (station → Pi over WiFi/WireGuard) reuses these message shapes
over a WebSocket/MQTT frame instead of a line.

## Framing

One UTF-8 line per message. The link is shared with human log output, so:

| Prefix | Direction | Meaning |
|---|---|---|
| `>` | host → station | a command, `>` + compact JSON + `\n` |
| `<` | station → host | a reply, `<` + compact JSON + `\n` |
| anything else | station → host | log line — the collector ignores it |

Bare `a` `A` `b` `B` bytes (no `>`), outside a command line, still drive the
menu for desk testing. A command line over `kMaxLine` (1024) bytes is
dropped with `line_overflow`.

## Commands

Every command carries an integer `id`; the reply echoes it. `id` may be `0`
for fire-and-forget. Replies are `{"id":N,"ok":true,...}` or
`{"id":N,"ok":false,"error":"<code>"}`.

### `hello` — capability declaration (ADR-0003 §3)

```
>{"id":1,"op":"hello"}
<{"id":1,"ok":true,"station":{
    "id":"stick-6e27ac","model":"m5stack-sticks3",
    "fw_version":"0.1.0 v0.1.0-8-ga03f79d5-dirty",
    "toolsets":["wifi","ble","ir","sensors"],
    "radios":["wifi-2g4","ble","ir-tx","ir-rx"]}}
```

`id` is `stick-` + the low 6 hex of the eFuse MAC — stable per device, no
NVS. `toolsets` is `registry.toolsets()`; `radios` is static for now.

### `status`

```
>{"id":2,"op":"status"}
<{"id":2,"ok":true,"uptime_s":23,"heap":286860,"batt":99,"on_usb":false,
  "awake":true,"capture":{"frames":0,"used":0,"cap":0,"recording":false,"full":false}}
```

### `tools` — the registry (ADR-0004)

```
>{"id":3,"op":"tools"}
<{"id":3,"ok":true,"tools":[
    {"id":"sensors.motion","name":"Motion / Tamper","toolset":"sensors",
     "summary":"BMI270 movement watch","enabled":true,"has_run":true}, ...]}
```

### `run` — headless dispatch through `Registry::run`

```
>{"id":4,"op":"run","tool":"sensors.motion","params":{}}
<{"id":4,"ok":true,"result":{"ax":-0.13,"ay":-0.99,"az":0.13,
    "accel_dev_g":0.006,"gyro_dps":0.3,"moving":false,"face":"bottom"}}
```

`params` (object or omitted) is serialized and passed through as the opaque
params string. Errors, matching the `RunOutcome` → MCP vocabulary:

| `error` | when |
|---|---|
| `capability_not_present` | no such tool id |
| `capability_disabled` | tool toggled off |
| `device_only` | registered + enabled, but no `Tool::run` yet (menu-only) |
| `missing_tool` | no `tool` field |

Today only `sensors.motion` / `sensors.gesture` return `ok`; every other
tool is `device_only` until it grows a headless `run()` (ADR-0003 §8).
Emitting tools stay behind arm/confirm regardless — a headless fire will
need an explicit confirm token (ADR-0008 §1).

### `set` — enable / disable a tool (persists to NVS)

```
>{"id":5,"op":"set","tool":"ir.tvbgone","enabled":false}
<{"id":5,"ok":true,"enabled":false}
```

### `stream_start` / `stream_stop` / `stream_status` — the streaming tools

One background capture session at a time (single radio, single loop). While
active the station emits `!`-prefixed NDJSON: an `{"event":"open"}` line, one
strike per event, an `{"event":"close","count"}` line on stop. Strike
envelope per `strike-format.md` (device `ts` is boot-relative `"t+<ms>"`; the
collector normalizes on ingest).

```
>{"id":6,"op":"stream_start","tool":"wifi.monitor","params":{}}
<{"id":6,"ok":true,"flash":"cf5e10"}
!{"flash":"cf5e10","event":"open","tool":"wifi.monitor"}
!{"v":1,"flash":"cf5e10","id":"cf5e10-0","station":"stick-6e27ac","ts":"t+13886",
  "toolset":"wifi","kind":"monitor",
  "payload":{"decoded":{"total":92,"mgmt":69,"data":23,"ctrl":0,"rate":83,"rssi":-59}}}
 …one per second…
>{"id":7,"op":"stream_stop"}
!{"flash":"cf5e10","event":"close","count":10}
<{"id":7,"ok":true,"count":10}
```

`stream_start` errors: `busy` (a session is already running), `unknown_tool`,
`capability_disabled`, `start_failed`, `missing_tool`. `stream_stop` on an
idle channel returns `{"ok":false,"error":"not_streaming"}`. `stream_status`
→ `{active, tool, flash, count}`.

| tool | strike `kind` | payload |
|---|---|---|
| `wifi.monitor` | `monitor` (~1/s) | `{total,mgmt,data,ctrl,rate,rssi}` |
| `wifi.deauth` | `deauth` (on threshold cross, re-armed once calm) | `{bssid,deauth,disassoc,rate}` |
| `wifi.handshake` | `eapol` per frame, `handshake` at 4 EAPOL for a BSSID | `{bssid,ssid,rssi,eapol}` / `{bssid,ssid,frames,bytes}` |

`ble.spam`, `sensors.mic`, `wifi.wardrive` follow the same pattern (not yet
wired).

## Not yet

A framed `op` for the pcap buffer (`wifi.handshake` fills it) — pull is still
the base64 `---PCAP-BEGIN/END---` export from `Settings > Capture`.
