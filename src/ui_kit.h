#pragma once

#include <cstdint>

// Shared immediate-mode drawing grammar (ADR-0011 §2). Every Screen composes
// from these; none calls gfx() with a literal colour or hand-rolls the title
// / hint bars again. Metrics target the 240x135 panel.
namespace kit {

constexpr int W = 240;
constexpr int H = 135;
constexpr int STATUS_H = 18;  // persistent status strip (menus)
constexpr int TITLE_H = 20;   // per-screen title bar (tool detail screens)
constexpr int HINT_H = 12;
constexpr int BODY_Y = TITLE_H;
constexpr int BODY_H = H - TITLE_H - HINT_H;

constexpr int ROW_H = 24;   // list row, fits size-2 text + a 20px icon
constexpr int STAT_H = 20;  // key/value row

int listRows();  // BODY_H / ROW_H
int statRows();  // BODY_H / STAT_H

// Top strip: `bgAlt` ground, `text` in `fg` (size 1), a 2px `accent` rule
// beneath. Pass theme fg for a plain screen, toolsetAccent(...) for a tool.
void titleBar(const char* text, uint16_t accent);

// Bottom strip: `text` in `fgDim` on `bg` (size 1).
void hintBar(const char* text);

// Fill the region between the bars with `bg`.
void bodyClear();

// `titleBar` + `bodyClear` in one call; pair with `end(hint)`.
void begin(const char* title, uint16_t accent);
void end(const char* hint);

// Persistent top strip for menus: uptime clock left, battery / power right,
// `bgAlt` ground + accent rule. Screens draw it every frame.
void statusBar();

// One-item horizontal carousel (root + submenus): a big `iconKey` glyph in
// `accent`, `label` centred beneath it in size 3, and < > chevrons. Fills
// the region from STATUS_H to the hint bar.
void carousel(const char* iconKey, uint16_t accent, const char* label);

// One list row in slot 0..listRows()-1. `iconKey` (icon::draw key, e.g. a
// toolset id) draws a 20px glyph in `iconColor`; nullptr/"" = none. `dot`:
// 0 none, 'x' filled, 'o' hollow, drawn at the right edge in `dotColor`.
void row(int slot, const char* label, bool selected, const char* iconKey = nullptr,
         uint16_t iconColor = 0, char dot = 0, uint16_t dotColor = 0);

// `label` (fgDim) … `value` (valColor, 0 = fg), size 2, in stat slot.
void stat(int slot, const char* label, const char* value, uint16_t valColor = 0);

// Draw `text` at (x, y) size `size` in `color`, clipped to `boxW` px. If it
// overflows, it scrolls ("marches") left/right, pausing at each end. `id`
// (0..3) keys an independent scroll state — one per marquee on screen. Reset
// happens automatically when the text changes.
void marquee(const char* text, int x, int y, int boxW, int size, uint16_t color,
             int id = 0);

// True while any marquee is mid-scroll — a screen with a marquee should set
// dirty_ every frame while this holds so the scroll is smooth.
bool marqueeAnimating();

// The list-detail body used by the record screens (scan / wardrive / probe /
// handshake / gatt): a big marqueed headline, a size-1 sub-line, and an
// `n/total` counter. Pair with statusBar()/titleBar() + hintBar().
void detail(const char* headline, const char* subline, int n, int total);

// One large centred value (size 3) in an ok/warn/bad colour.
void bigValue(const char* text, uint16_t color);

// Verdict-screen body: `headline` size 3 near the top, then up to 3 size-2
// `line(slot, ...)` rows beneath it. Call `bodyClear()` / titleBar / hintBar
// around them.
void headline(const char* text, uint16_t color);
void line(int slot, const char* text, uint16_t color = 0);  // slot 0..2

// Bar spectrum across the body. `mag[0..n)` scaled to `norm`.
void spectrum(const float* mag, int n, float norm);

}  // namespace kit
