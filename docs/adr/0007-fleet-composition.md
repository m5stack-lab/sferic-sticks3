# ADR-0007 — Fleet composition, per-class roles, and the collector stack

**Status:** Accepted as direction. **Implementation deferred** — only the
StickS3 firmware is being built now. This ADR records the target so the
firmware stays portable and the server work has something to aim at.
**Date:** 2026-08-31
**Relates to:** ADR-0002 (name / vocabulary), ADR-0003 (companion MCP
server), ADR-0004 (tool registry & control surfaces), ADR-0006 (web UI)

## Context

The fleet is concrete now, and it is **tiered** — not a flat mesh of
identical cheap nodes. Each device class earns a distinct job. Writing this
down constrains the firmware (keep the HAL board-agnostic) and gives the
Pi-side work a fixed target, without starting any of it yet.

## Decision

### 1. Fleet roster & roles

| Device | Class | Role |
|---|---|---|
| **Raspberry Pi 5** | Linux, always-on | **Collector / control plane.** The ADR-0003 companion MCP server. Durable storage, cross-station correlation (time-of-arrival, dedup), pcap indexing, capture→analyze→crack job queue, dashboard host, MCP endpoint for the agent. |
| **MacBook Air** | Linux/mac, operator | **Operator console.** Runs the agent (Claude Code) as an MCP client to the Pi. Optionally a station too — native monitor-mode WiFi, BLE, large disk. |
| **M5Tab5** | ESP32-P4 + ESP32-C6 | **Field command station.** Rich touch UI (fleet map, not just a sensor). The C6 brings **802.15.4 / Thread / Zigbee** — a toolset no other node has. USB 2.0 host → clean BadUSB (no S3 USB-mode fork). Camera → QR / visual. Can relay for ESP32 stations out of Pi range. |
| **M5Cardputer ADV** | ESP32-S3 | **Portable terminal.** Keyboard → real BadUSB payloads, on-device scripting, text entry. microSD → local capture persistence. IR. The walk-around node. |
| **M5StickS3 + RF Pack S3** | ESP32-S3 | **Disposable sensor.** SubGHz (CC1101) + 2.4 GHz (nRF24L01+) via the backpack, tiny, battery, leave-behind. The device being built now. |

### 2. Two firmware classes, one codebase

- **Constrained station** (ESP32-S3: Stick, Cardputer ADV): the current
  `sferic` firmware. Small screen, menu-driven, radio-forward.
- **Rich station** (ESP32-P4: Tab5): the same firmware, with a
  **resolution-scaled UI** (list + detail side-by-side on 1280×720, not one
  screen at a time) and extra toolsets (Zigbee/Thread, camera, USB host).

Both declare their `{ toolsets, radios }` on connect (ADR-0003 §3); the
collector exposes only what each supports. Per-class toolsets get their own
`docs/mcp/` contracts when those devices come online.

### 3. Collector stack (Raspberry Pi 5) — deferred

- **MCP server** — the control plane (ADR-0003). Exposes fleet + per-station
  tools to the agent; owns strike/flash storage.
- **Redis** — hot fleet state (stations online, last-seen, current
  activity), pub/sub for live strike streaming to the dashboard/agent, and
  the job queue (capture → analyze → crack).
- **Qdrant** — vector store for semantic recall over captured artifacts and
  session observations (device fingerprints, SSIDs, signal signatures,
  notes): "seen this device before / near here", cross-session correlation,
  agent context. (Mirrors the operator's existing semantic-memory pattern.)
- **Dashboard** — served from the Pi; the ADR-0006 web UI grows into the
  fleet-wide view.
- A durable relational store may be added later for long-term history;
  Redis + Qdrant cover hot state and recall.

### 4. Network

- **Tailscale** joins the Pi 5, the MacBook, and any Linux station into one
  tailnet. The operator reaches the Pi's MCP server from anywhere; the Pi
  can act as a subnet router for the local station segment.
- **ESP32 stations** connect to the Pi over local WiFi (or a Tab5 relay when
  out of range). The **station ↔ collector link** is a persistent
  **WebSocket or MQTT to the Pi** (ADR-0003 §6 — decide at implementation;
  both fit a Pi cleanly).

### 5. Edge (Cloudflare Workers) — optional, deferred

For the remote / public-facing surface only: static dashboard hosting, alert
relay (station event → notification), and optionally an MCP-over-HTTPS front
so the agent doesn't need Tailscale. The Pi stays the source of truth; the
Worker is cache / relay / notifier.

### 6. CI (GitHub Actions)

Build every firmware env on push (later: a per-board matrix). Tags cut
release `.bin` artifacts. Lint / format gate. The Pi server gets its own
pipeline when it exists.

## Consequences

- **Firmware constraint, effective now:** no StickS3-specific assumptions
  leak past `src/hal.cpp`; the UI stays resolution-driven (it already uses
  `hal.w()/h()`). A `board_M5CardputerADV` build check is worth doing the
  next time the HAL is touched — not a task now.
- Everything Pi-side, edge-side, and non-StickS3-device is **deferred**. The
  current milestone is the StickS3 firmware + its local web UI.
- `docs/mcp/` contracts + capability declaration are the integration seam.
- Repo layout: this firmware is `ry-ops/sferic-sticks3` (renamed from
  `ry-ops/sferic` on 2026-08-31); the collector is its own repo
  `ry-ops/sferic-collector` (ADR-0003). Future per-device firmware follows
  `ry-ops/sferic-<device>`. A dedicated `sferic` org is planned. See
  ADR-0002's 2026-08-31 update.

## Alternatives considered

- **Flat mesh of identical cheap nodes.** Rejected — the heterogeneity is
  the point; each tier does what it's best at.
- **Everything self-hosted, no Cloudflare.** Viable; not committed either
  way. CF is cheap remote access + alerting if wanted.
- **Postgres instead of Redis + Qdrant.** Redis for hot state, Qdrant for
  semantic recall; a relational store may still join for durable history.
- **Skip Tailscale, expose the Pi directly.** Rejected — Tailscale is the
  low-friction secure path for a fleet that moves between networks.
