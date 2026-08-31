# ADR-0008 — Control plane split & station network uplink

**Status:** Accepted (direction). Web-UI side is live; MCP + uplink work is
deferred with the rest of the collector.
**Date:** 2026-08-31
**Deciders:** ry-ops (operator)
**Relates to:** ADR-0003 (companion MCP server), ADR-0004 (tool registry &
control surfaces), ADR-0006 (web UI), ADR-0007 (fleet composition)

## Context

Two questions came up once the headless `run()` seam landed (ADR-0004 §4)
and the web UI could technically launch tools:

1. **Which surface runs what?** The device menu, the web UI, and the MCP
   server are all views over the registry (ADR-0004 §2). The web UI now has
   Run buttons. Does it run *everything*, or is MCP the real control plane?
2. **Can a station reach the tailnet directly?** ADR-0007 §4 puts the Pi and
   the Mac on Tailscale. Could the StickS3 itself be a tailnet node, so it
   phones home from any network?

Both answers fall out of one physical fact: **an ESP32-S3 station has one
2.4 GHz radio and no OS.**

## Decision

### 1. MCP server is the full control plane; the web UI runs a subset

- **MCP server (Pi collector)** runs **every** tool headlessly, including
  `wifi.*` and `ble.*`. The station reaches the collector over its normal
  STA-WiFi client connection (ADR-0003 §6), so the SoftAP is not involved
  and there is no radio conflict. MCP is also the only surface for
  fleet-wide runs, queued jobs (capture → analyse → crack), strike
  streaming, and cross-station correlation.
- **Web UI** runs only the tools that do **not** need the 2.4 GHz radio,
  because its own SoftAP is using it while the page is up (ADR-0006 §1):
  - now: `sensors.*` (I2C)
  - later: `ir.*` (GPIO), `subghz.*` / `nrf24.*` (RF Pack, SPI) — separate
    silicon, no conflict
  - never from the web UI: `wifi.*`, `ble.*`
- The web UI **must not** fake WiFi/BLE runs by cycling its own AP
  (stop AP → run → restart AP). It drops the operator's connection every
  time and is fragile. A tool that needs the radio the page is using is
  simply not runnable from that page — surface it as a disabled Run control
  with a "radio in use — run via MCP" hint, not a hidden one.
- **Emitting tools** (`ir.tvbgone`, `ir.capture` replay, `*.hid`, future RF
  TX) stay behind the on-device two-step arm/confirm guard (`src/confirm.h`)
  on **every** surface. A headless launch of an emitter either has no
  `run()` or requires an explicit confirm token — never a one-call fire.
  (The "woke the house" rule.)

This matches the `Tool::run == nullptr` convention already in the firmware:
radio-and-SoftAP-conflicting or emitting tools carry a null `run` on the
station; the collector supplies their headless path.

### 2. No Tailscale client on stations

- **`tailscaled` / `tsnet` do not run on ESP32-class hardware.** They are
  Go, need an OS and tens of MB of RAM, and there is no port (TinyGo cannot
  carry `tsnet`). This applies to every current and planned station
  (StickS3, Cardputer ADV, Tab5 / ESP32-P4). Full Tailscale lives on the
  **Pi 5 and the MacBook** only.
- **Default station uplink: Pi 5 as a Tailscale subnet router.** The station
  is a plain WiFi client on a local segment; the Pi runs
  `tailscale up --advertise-routes=<segment>` and operators reach the
  station *through* the Pi (ADR-0007 §4). **Zero firmware work.** The
  station has no tailnet identity of its own.
- **Optional station uplink: point-to-point WireGuard to the Pi.** For
  stations that roam between networks and must still reach the collector,
  add a single-peer WireGuard tunnel to the Pi
  (`ciniml/WireGuard-ESP32-Arduino`, the library ESPHome vendors). This is
  *"WireGuard to the collector"*, **not** a tailnet node — no MagicDNS name,
  no admin-console presence, no independent ACL identity; it rides the Pi's.
  - Cost: ~40 KB flash, a few KB RAM, one lwIP netif, and SNTP before the
    handshake (over plain WiFi first — no chicken-and-egg).
  - Limits: one peer, no DERP, no NAT traversal, no roaming of the *tunnel*
    itself — the Pi endpoint must be reachable. Soft crypto on the S3 →
    a few Mbit/s (fine for strikes/telemetry, slow for bulk pcap).
  - Gated behind a build flag / an explicit "uplink mode"; mutually
    exclusive with the SoftAP web UI and with promiscuous WiFi capture, like
    every other radio user.
- Self-hosting the coordination server (headscale on the Pi) is a Pi-side
  choice and changes nothing here — a station still cannot run the client.

## Consequences

- **Now (web UI):** keep Run buttons scoped to `has_run` tools; when a
  WiFi/BLE tool is shown, render its Run control disabled with the "run via
  MCP" hint rather than omitting it. No change to `Registry::run` — the
  `NoHeadlessPath` outcome already covers device-only tools.
- **Collector (deferred):** the MCP server owns the headless path for
  `wifi.*` / `ble.*` and every emitting tool's confirm flow. `docs/mcp/`
  contracts describe the full surface; the station's capability declaration
  (ADR-0003 §3) still gates what the server exposes per station.
- **Uplink (deferred):** the station↔collector link (ADR-0003 §6) is
  WebSocket/MQTT over either plain local WiFi (subnet-router path) or the
  optional WireGuard tunnel. Decide the wire format when the server is
  built; the transport choice does not depend on which uplink is used.
- A future `wireguard` build flag + config (peer key, endpoint, allowed
  IPs) joins the `*.example` secret-handling rule — keys never committed.

## Alternatives considered

- **Web UI runs everything, cycling its own AP for WiFi/BLE tools.**
  Rejected: every such run drops the operator's connection; racey AP
  teardown/restart; no way to stream. MCP-over-STA is the clean path and
  already the model.
- **On-device MCP endpoint per station** (revisit of ADR-0003's rejected
  alternative). Still rejected as the primary model; the web UI already
  covers "next to the device with a phone".
- **Full Tailscale on the station.** Not possible — see §2.
- **WireGuard to the Pi as the default** (not optional). Rejected for now:
  most deployments have the station and Pi on the same LAN, where the
  subnet-router path is simpler and needs no station-side crypto or key
  management. Promote to default only if roaming stations become common.
- **Stations talk to the tailnet via the Tab5 as a relay.** Possible later
  (ADR-0007 notes the Tab5 can relay for out-of-range ESP32s), but the Tab5
  is not a tailnet node either — it would relay to the Pi, which is just the
  subnet-router path with an extra hop.
