# ADR-0004 — Tool registry & the three control surfaces

**Status:** Accepted. Device side implemented this cycle; web UI + MCP are
contracts here, built later.
**Date:** 2026-08-30
**Deciders:** ry-ops (operator)
**Relates to:** ADR-0001 (tool catalog), ADR-0003 (MCP server)

## Context

Tools are currently hard-wired into `menu_tree.cpp`. The catalog in ADR-0001
is ~25 tools across many toolsets. Users need to hide what they don't use,
disable risky tools, and do so from any of three places: the **device UI**,
a **web UI** (not yet designed), and the **MCP server**. Three surfaces
mutating three copies of "which tools are on" would drift.

## Decision

### 1. Every tool is a registered entry

```
Tool {
  id        // "<toolset>.<name>", matches the MCP tool name where 1:1
  name      // menu label, e.g. "AP Scan"
  toolset   // "wifi" | "ble" | "ir" | ...
  summary   // one line
  make()    // -> Screen*  (on-device view)
  // run(params) -> result   -- added later, ADR-0003 §8 headless path
}
```

Registered at startup into a single **`Registry`**. Adding a tool is one
`registry.add({...})` line plus its `Screen`.

### 2. `enabled` flag, persisted, one source of truth

Each tool has an `enabled` bool, default `true`, persisted in NVS
(`Preferences`, namespace `sferic`, key `tools.disabled` = the set of
disabled ids — absence means enabled). The `Registry` is the **single source
of truth**. The three surfaces are views/controllers over it and contain
**no tool logic**:

| Surface | Reads | Writes |
|---|---|---|
| **Device UI** | menu is generated from *enabled* tools, grouped by toolset | `Settings > Tools` toggles `enabled` |
| **Web UI** (station-hosted; design = a later ADR) | lists all tools + state; launches / streams a tool | toggles `enabled` |
| **MCP server** | `tools/list` returns *enabled* tools only | `sferic.tool_enable` / `sferic.tool_disable` (per-station or fleet) |

### 3. Enable/disable semantics

- Disabled ⇒ absent from the device menu, absent from MCP `tools/list`,
  greyed in the web UI.
- Calling a disabled tool over MCP returns `capability_disabled` (distinct
  from `capability_not_present`, which means the station's hardware/toolset
  can't do it at all).
- Disabling a **running** tool stops it at the next tick (radio powered
  down, screen popped).
- A toolset-level toggle is sugar over the per-tool flags ("disable all
  `wifi.*`").

### 4. Headless alignment (ADR-0003 §8)

The same registry entry eventually carries `run(params) -> result`. Menu,
web, and MCP then all dispatch through one object; the `Screen` is a thin
view over it.

## Consequences

- **This cycle:** `src/tool.h` + `src/registry.{h,cpp}`; tools register at
  boot; `menu_tree` builds the menu from the registry; `Settings > Tools`
  toggle screen; `MenuScreen` gains a live-rebuild constructor so toggles
  take effect on the next visit.
- Every new tool: `registry.add({...})` + a `Screen` (+ later `run`).
- NVS write per toggle. Namespace `sferic`, key `tools.disabled`.
- `docs/mcp/*` gain the `capability_disabled` error and note that
  `tools/list` is filtered.
- The **web UI** gets its own ADR when designed; this ADR fixes its
  contract: a registry view, no logic of its own, same enable/disable
  semantics.

## Alternatives considered

- **Compile-time enable** (a build flag per tool). Rejected: no runtime
  toggle from any surface; needs a rebuild + reflash.
- **Per-surface enable state.** Rejected: they drift; the entire point is
  one source of truth.
- **A plugin/DSO model** (load tools at runtime). Rejected: massive
  overkill on an MCU; a compiled-in registry with a runtime flag gives all
  the control we need.
