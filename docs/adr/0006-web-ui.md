# ADR-0006 — Web UI

**Status:** Accepted
**Date:** 2026-08-30
**Relates to:** ADR-0003 (MCP server), ADR-0004 (tool registry & control
surfaces)

## Context

ADR-0004 named three control surfaces over the registry: the device menu,
the MCP server, and a **web UI** — the last one only a contract so far
("a registry view, no logic of its own, same enable/disable semantics").
Time to define and build it.

Constraints:

- The 2.4 GHz radio is a **shared, exclusive resource**. A promiscuous
  capture and a SoftAP-hosted web server cannot run at the same time.
- No keyboard on the device — a STA mode web UI would need credentials
  entered somehow.
- The headless `run(params)` path (ADR-0003 §8 / ADR-0004 §4) does not exist
  yet, so the web UI cannot *launch* tools in v1.

## Decision

### 1. A menu entry, not an always-on service

`Settings > Web UI` starts a **SoftAP** (`sferic-ui`, open) + an HTTP server
on `192.168.4.1`. While it is up it is the exclusive WiFi user; WiFi tools
are unavailable until you leave the screen (and vice versa). Exit tears the
AP + server down and frees the radio. A catch-all DNS makes any URL open the
UI (captive portal).

### 2. v1 scope: monitor + configure + capture hand-off

The web UI **reads the registry and station state, toggles tool
enable/disable, and downloads captures**. It does **not** launch tools —
that waits for the headless path. The on-device menu remains the way to run
a tool.

### 3. HTTP surface

| Route | Purpose |
|---|---|
| `GET /` | the SPA — one self-contained HTML page, no external assets, served from PROGMEM |
| `GET /api/status` | `{ name, version, build, uptime_s, heap, batt, on_usb, capture:{frames,used,cap,recording,full} }` |
| `GET /api/tools` | `[{ id, name, toolset, summary, enabled }]` — straight from `Registry` |
| `POST /api/tools/set?id=<id>&on=<0\|1>` | `registry.setEnabled()` — same path as `Settings > Tools`, persists to NVS |
| `GET /api/capture.pcap` | the shared pcap buffer as a binary download (`application/vnd.tcpdump.pcap`) — the real capture hand-off, replacing base64-over-serial |
| `GET /api/wardrive.csv` | (later) the wardrive log |

No auth in v1 — it's an open AP you physically stood up. If that changes,
a token goes in `/api/status` and every mutating call.

### 4. Contract compliance (ADR-0004)

The web UI holds **no tool logic**. Every mutation goes through `Registry`.
Enable/disable semantics are identical to the device and MCP surfaces
(hidden in menu, absent from MCP `tools/list`, `capability_disabled`).

## Consequences

- `src/webui.{h,cpp}` (`WebUiScreen` + SoftAP + `WebServer`) and
  `src/webui_html.h` (the SPA as a PROGMEM string).
- `PcapBuffer` gains a `raw()` accessor for the binary download.
- The SPA polls `/api/status` (~2 s) and re-fetches `/api/tools` after a
  toggle. Mobile-first (you reach it from a phone).
- When the headless `run()` path lands, `POST /api/tools/<id>/run` is added
  and the SPA grows a "run" button per tool — no change to this ADR's
  structure.
- If a station is later connected to the MCP server over WiFi-STA, the web
  UI could be served on that interface too; v1 is SoftAP only.

## Alternatives considered

- **Always-on SoftAP web UI.** Rejected: permanently blocks every WiFi tool.
- **STA mode, join a known network.** Rejected for v1: no way to enter a
  password on the device; revisit once the MCP link exists.
- **Full tool control in v1.** Rejected: needs the headless refactor first;
  shipping config + capture-download now is honest and unblocks the useful
  90% (pull your pcap off the device over WiFi instead of serial).

## Update — 2026-08-31

- **Page sources moved out of the C string.** `web/{index.html,style.css,
  app.js}` are the sources; `scripts/gen_webui.py` inlines them into
  `src/webui_html.h` (a `pre:` extra_script on the firmware envs, and a CI
  check that the committed header matches). Don't hand-edit the header.
- **Tool launch landed.** The headless `run()` path exists (ADR-0004 §4), so
  the SPA now shows a **Run** button on `has_run` tools → `GET
  /api/tools/run?id=<id>` → result rendered inline. `/api/tools` carries
  `has_run`. (Route is `/api/tools/run`, not the `/api/tools/<id>/run`
  guessed above.)
- **Display controls.** `/api/status` gained `display:{sleep_ms,brightness,
  awake}`; `POST /api/display?sleep_ms=&brightness=` mirrors `Settings >
  Display`. The page has Sleep / Brightness selectors.
