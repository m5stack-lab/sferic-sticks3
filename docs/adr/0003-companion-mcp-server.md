# ADR-0003 — Companion MCP server

**Status:** Accepted (shape + rules). Implementation deferred — no server
code yet.
**Date:** 2026-08-30
**Deciders:** ry-ops (operator)

## Context

`sferic` is a fleet of **stations** (ADR-0002). Today that fleet is one
M5Stack StickS3. It will grow to other M5 devices — Cardputer, AtomS3,
Dial, StampS3 — each with different hardware and therefore a different set
of toolsets.

We want an agent (Claude) to drive the fleet: *"scan wifi on the station by
the garage"*, *"capture 433 for 30s into a flash"*, *"replay strike 3"*.
That means the tools need a clean, describable, headless interface — and it
is far cheaper to design that now, while tools are small, than to
reverse-engineer it from firmware later.

The MCP server itself is not needed yet. This ADR fixes what it *is* so the
firmware is built to fit it.

## Decision

### 1. There is one companion MCP server — a host-side collector + control plane

Not on-device. A host process that is the fleet's collector: stations
connect to it, MCP clients (agents) connect to it, it holds fleet state and
strike/flash storage and does any cross-station work (time-of-arrival
correlation, dedup). It talks only to a station's **headless command
interface**, never its on-device UI.

```
station ──link──┐
station ──link──┼──▶  MCP server  ◀──MCP──  agent / CLI
station ──link──┘         │
                      strike/flash store
```

### 2. Addressing

Every MCP tool that touches hardware takes a **`station`** argument — an id
or a friendly name. Where a fleet-wide action makes sense (`sferic.stations`,
a coordinated multi-station scan) `station` may be `"*"` or omitted.

### 3. Capability declaration

On connect a station announces:

```json
{ "id": "...", "model": "m5stack-stickс3", "fw_version": "...",
  "toolsets": ["wifi", "ble", "ir", "sensors"],
  "radios": ["wifi-2g4", "ble", "ir-tx", "ir-rx"] }
```

The server exposes only the MCP tools whose toolset the *target* station
declares. Calling an unsupported tool returns a typed
`capability_not_present` error. **This is how new M5 device types slot in
with no server change** — they just declare a different toolset list.

### 4. Vocabulary is the data model (ADR-0002)

- **Station** — one device.
- **Strike** — one captured event: `{ station, ts, toolset, payload, meta }`.
- **Flash** — a named/id'd collection of strikes from a capture session.
- **Tweek** — the replay / transform path (a strike re-emitted or reshaped).
- **Whistler** — reserved (long-path capture mode).

These are the nouns in tool names and return shapes.

### 5. Tool naming

`<toolset>.<verb>` — `wifi.scan`, `ble.scan`, `ir.capture`, `ir.replay`,
`subghz.capture`. Fleet-level: `sferic.stations`, `sferic.flash`,
`sferic.strikes`.

### 6. Transport — roles fixed, bytes deferred

- **station ↔ server:** a persistent link. Candidates (decide at
  implementation): station opens a WebSocket to the server; MQTT; a
  serial/USB link for a tethered station. The ADR fixes that the station is
  the client and the link is persistent — not the wire format.
- **MCP client ↔ server:** standard MCP (stdio locally; streamable HTTP for
  remote).

### 7. The rule: every toolset ships an MCP contract doc

Every firmware toolset MUST have a matching **`docs/mcp/<toolset>.md`** that
defines its MCP tool calls (name, params, returns, errors), kept in sync
with the firmware tool as it is built. The on-device tool and the MCP tool
are two front-ends over one capability; if they cannot share a data shape,
the capability is under-designed. These docs are **living specs, not ADRs** —
they change freely; promote something to an ADR only on a real design fork.

### 8. Headless-ready firmware

As toolsets are built, capability logic is kept separable from the `Screen`
so the same code can be driven by the on-device menu *and* a headless
command path. Not required to build the headless path now — but do not bury
scan/capture logic inside `draw()`. The `Screen` should become a thin view
over a plain tool object.

## Consequences

- No server code this cycle. This ADR is the target the firmware aims at.
- Every new toolset PR includes its `docs/mcp/<toolset>.md`.
- Existing tools (`wifi`, `ble`) get backfill docs now as the template.
- **Strike/Flash storage format** must be defined before the first capture
  tool (IR) — follow-up, likely `docs/mcp/strike-format.md`.
- Firmware tools get a light capability/UI split over time; WiFi/BLE scan
  are simple enough to leave until the headless path is built.
- Whether the server lives in this repo (`server/`) or its own repo is a
  later decision.

## Alternatives considered

- **On-device MCP server** — each station hosts MCP over its own WiFi AP.
  Rejected as the primary model: no fleet, no cross-station correlation,
  every station a silo, the agent must track N endpoints. A single-station
  direct mode could still be offered later as a convenience.
- **No MCP — a CLI that serials/SSHes into each station.** Rejected: loses
  the structured tool surface, capability negotiation, and the agent-native
  interface that is the point.
- **An ADR per toolset** (the original proposal). Rejected: interface specs
  churn while tools are half-built; ADRs are meant to be stable. Living
  `docs/mcp/` docs instead.
