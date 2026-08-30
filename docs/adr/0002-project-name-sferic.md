# ADR-0002 — Project name: `sferic`

**Status:** Accepted
**Date:** 2026-08-30
**Deciders:** ry-ops (operator)
**Supersedes:** the placeholder repo name `ry-ops/new`

## Context

The project needs a real name. The working title `new` is a placeholder.

The system we are building is, architecturally, a **lightning-detection
network**: distributed sensor stations each timestamp an impulse and report
to a central collector, which does time-of-arrival triangulation to place
the source. That is exactly the M5 device fleet + MCP server topology in
ADR-0001. A **sferic** (atmospheric radio impulse — "atmospheric" →
"sf-eric") is the fundamental unit those networks detect: a broadband RF
burst. Every tool in ADR-0001 captures, replays, or analyzes an RF impulse
of some kind, so the term is literal, not decorative, and its field
vocabulary maps cleanly onto our domain model.

Name availability checked 2026-08-30:

| Registry | `sferic` | Note |
|---|---|---|
| GitHub `ry-ops/sferic` | free (404) | `github.com/sferic` is an empty user account — irrelevant, we namespace under `ry-ops` |
| npm | free (404) | |
| PyPI | free (404) | |
| crates.io | free (404) | |

**Confusable neighbors** (not disqualifying, worth knowing):

- **`sferik`** — one letter off. Erik Berlin, a well-known Ruby developer
  with 200+ repos. Search engines and shell autocomplete will occasionally
  mix them up.
- **`sferes2`** — an unrelated C++ evolutionary-computation framework, in
  the fuzzy-match neighborhood.

## Decision

The project is named **`sferic`** (lowercase for the repo, CLI, and package;
"Sferic" in prose).

Adopt the lightning-detection vocabulary as the domain model — these are
real terms of art in the field:

| Term | Meaning here | Origin |
|---|---|---|
| **Station** | one M5 device in the fleet | sensor station in a detection network |
| **Strike** | one detected / captured event | a single lightning strike |
| **Flash** | a session grouping multiple strikes | a lightning flash contains many strokes |
| **Tweek** | the replay / analysis path — a signal that has "been through something" | a sferic that dispersed over distance and rings with a tail |
| **Whistler** | reserved for a long-tail / long-path capture mode, whatever that becomes | a sferic that followed magnetic field lines and returned descending in frequency |

CLI surface (for the host companion / MCP tooling):

```
sferic stations          list the fleet + last-seen
sferic listen            live feed of strikes from all stations
sferic capture [opts]    record strikes to a flash
sferic replay <id>       re-emit a captured strike (the "tweek" path)
sferic flash <id>        inspect / export a flash
```

## Consequences

- **Repo rename:** `ry-ops/new` → `ry-ops/sferic`; local dir
  `~/Projects/new` → `~/Projects/sferic`; update the git remote. (Follow-up,
  not done by this ADR.)
- Reserve `sferic` on npm, PyPI, and crates.io if/when a package ships, to
  keep it away from the `sferik` confusion.
- Firmware code, on-device menu title, and docs adopt the vocabulary:
  a device is a *station*, a captured event is a *strike*, a capture session
  is a *flash*. ADR-0001's tool catalog is unchanged — only the framing
  language updates.
- `README`, `platformio.ini` env name, and the on-screen title get the
  `sferic` name on the next pass.
- Accept the low-grade `sferik` search collision as a known cost.
