# ADR-0011 — On-device UI kit, theme, and icon set

**Status:** Accepted. `src/theme.{h,cpp}` + the `Settings > Display` toggle
landed as the preview; the `ui_kit` sweep and icon set follow.
**Date:** 2026-09-02
**Deciders:** ry-ops (operator)
**Relates to:** ADR-0001 (tool catalog — RF Pack tools inherit this),
ADR-0004 (registry / `toolset` ids), ADR-0006 (web UI — separate surface,
not themed by this)

## Context

The on-device UI is 18-plus `Screen` subclasses, each hardcoding
`TFT_BLACK` / `TFT_DARKGREY` / `TFT_NAVY`, cursor math, and `setTextSize(1)`.
Three problems: (1) it's small — the operator wants larger text and icons
for readability; (2) there's no shared look, so every tool re-invents the
title bar and hint line; (3) the RF Pack S3 tools (`subghz`, `nrf24`) are
coming and would inherit the copy-paste.

The display is an **ST7789P3, 240×135 landscape** — tiny. At the 6×8 font
(`setTextSize(1)`) that's ~40 chars × ~8 rows; at 12×16 (`size 2`) it's
~20 × ~4. Flash and PSRAM are both <45 % used, so asset room is not a
constraint; screen real estate is.

## Decision

### 1. Theme is a set of roles, one active palette

`src/theme.h` — a `Theme` struct of **roles, not hues**:

```
bg  bgAlt  fg  fgDim  selFg  selBg  line
ok  warn  bad
accent{Wifi,Ble,Ir,Sensors,Subghz,Nrf24}
```

Two variants ship: **Solarized Dark (default)** and Solarized Light,
`constexpr` RGB565, indexed 0/1, selected in `Settings > Display` and
persisted to NVS (`sferic` / `disp.theme`). `theme()` is the active palette;
`toolsetAccent("wifi"|…)` maps a `toolset` id (ADR-0004) to its accent.

- **Selection highlight is `selBg` — a neutral tone**, not an accent, so the
  per-toolset accent stays free for icons and status.
- **Status semantics are fixed:** `ok` = enabled / quiet / good, `warn` =
  caution, `bad` = alert / attack / armed. Tools use these three, never a
  raw colour.
- Adding a third palette later is one row in `kThemes[]`.

### 2. `ui_kit` — the drawing grammar

`src/ui_kit.{h,cpp}`. Every screen composes from these; none calls
`g.fillRect` / `g.setCursor` with literal colours again:

| helper | what |
|---|---|
| `titleBar(text, accent)` | top strip, `bgAlt` ground, `accent` rule under it |
| `hintBar(text)` | bottom strip, `fgDim` on `bg` |
| `bodyClear()` | fill the region between the bars with `bg` |
| `list(items, sel, top, iconFn?)` | the standard scrolling list — 24 px rows, `size 2` label, optional 24×24 icon in the toolset accent, neutral `selBg` bar, right-edge state dot |
| `tiles(items, sel)` | 2×3 grid of 64×48 icon tiles (the root menu) |
| `stat(label, value, row)` | a `label ……… value` line, `size 2` |
| `bigValue(text, status)` | one large `size 3` value in an `ok/warn/bad` colour (verdict screens) |
| `spectrum(bins, n, peak)` | the bar-graph primitive (mic, future SubGHz waterfall) |

### 3. Size & density — bigger by default, a per-screen escape hatch

- Menus, settings, and single-value / verdict tools use **`size 2`**.
- Data-dense tools (packet monitor counts, AP lists, probe collector) keep
  **`size 1` for the rows** but a `size 2` title and headline number, and
  scroll. They do **not** get shrunk to fit; long lists page.
- A per-`Screen` `dense_` flag toggles list row size where it's a genuine
  toss-up; default is `size 2`.

### 4. Icons — 1-bit glyphs, accent-coloured, `src/icons.h`

- One glyph per tool + one per toolset + a few UI glyphs (back, checkbox,
  active-dot). Hand-drawn, **24×24 for lists / 32×32 for the root grid**,
  stored as `const uint8_t NAME[] ` XBM (1 bpp; ~72–128 bytes each — the
  whole set is a few KB).
- Rendered with `drawXBitmap(x, y, bits, w, h, colour)` where `colour =
  toolsetAccent(tool.toolset)`. The glyph carries the accent; the theme
  swap re-tints it for free.
- `icons.h` maps `tool id -> const uint8_t*`. Adding an RF Pack tool = draw
  one glyph, add one row.
- Rationale over alternatives: a GFX/VLW **icon font** is one asset but
  needs a font-build toolchain and fixed baseline metrics; **PNG assets**
  need a decoder + RAM scratch and pull in file-system plumbing the device
  doesn't otherwise want (no SD). Compiled 1-bit arrays are the smallest,
  most direct, and trivially themeable option for two-tone art.

### 5. Scope

- **In:** the on-device `Screen` UI only — menus, tool screens, settings,
  About. RF Pack tools are built on `ui_kit` from day one.
- **Out:** the web UI (ADR-0006) keeps its own CSS; the serial/MCP surface
  has no UI. No animation framework, no compositor — immediate-mode draws
  as today, just centralised.

## Consequences

- `src/theme.{h,cpp}` + the toggle: **done** (`5de7837`).
- Next: `ui_kit.{h,cpp}`; retarget `ui.cpp` (`MenuScreen` / `InfoScreen`)
  onto it with the `size 2` bump and icon list; then `icons.h`; then a
  mechanical sweep of the ~18 tool `draw()`s (colours → roles, layout →
  `ui_kit`, per-toolset accent on the title bar). Each is its own small
  commit.
- The three build/verify gates are unchanged; add a `dense_` default check
  to the review checklist for new tools.
- `docs/` gets no new living spec — this ADR plus the `ui_kit` header
  comments are the reference.

## Alternatives considered

- **Keep per-tool drawing, just add a colour table.** Rejected: leaves the
  title/hint/list boilerplate duplicated 18× and forward into RF Pack.
- **A real widget/retained-mode UI toolkit (LVGL et al.).** Rejected:
  heavy for a 240×135 two-button device, and the current immediate-mode
  draws are fine once centralised.
- **Ship only one theme.** Rejected cheaply — the role struct makes the
  second palette ~free, and light-vs-dark on this panel is a real operator
  preference (glare vs. night).
- **Bitmap icon font.** Viable; deferred — 1-bit arrays need no toolchain
  and re-tint per theme without glyph metrics to manage.
