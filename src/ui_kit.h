#pragma once

#include <cstdint>

// Shared immediate-mode drawing grammar (ADR-0011 §2). Every Screen composes
// from these; none calls gfx() with a literal colour or hand-rolls the title
// / hint bars again. Metrics target the 240x135 panel.
namespace kit {

constexpr int W = 240;
constexpr int H = 135;
constexpr int TITLE_H = 20;
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

// One list row in slot 0..listRows()-1. `iconKey` (icon::draw key, e.g. a
// toolset id) draws a 20px glyph in `iconColor`; nullptr/"" = none. `dot`:
// 0 none, 'x' filled, 'o' hollow, drawn at the right edge in `dotColor`.
void row(int slot, const char* label, bool selected, const char* iconKey = nullptr,
         uint16_t iconColor = 0, char dot = 0, uint16_t dotColor = 0);

// `label` (fgDim) … `value` (valColor, 0 = fg), size 2, in stat slot.
void stat(int slot, const char* label, const char* value, uint16_t valColor = 0);

// One large centred value (size 3) in an ok/warn/bad colour.
void bigValue(const char* text, uint16_t color);

// Bar spectrum across the body. `mag[0..n)` scaled to `norm`.
void spectrum(const float* mag, int n, float norm);

}  // namespace kit
