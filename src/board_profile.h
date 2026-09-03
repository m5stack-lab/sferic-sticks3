#pragma once

// Compile-time board profile (ADR-0013 §4). The board-agnostic UI kit and
// HAL read their metrics + capability flags from here; a different board
// (e.g. sferic-tab5) ships its own copy of this header with different
// numbers, and nothing else in ui_kit / the tool draw() code changes.
//
// StickS3: 240x135 landscape, two buttons, no touch, no keyboard.

namespace board {

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

// Multiplier applied to font sizes and row/hit metrics in ui_kit for a
// larger panel. 1 = the original StickS3 layout.
constexpr int UI_SCALE = 1;

constexpr bool HAS_TOUCH = false;
constexpr bool HAS_KEYBOARD = false;
constexpr int PRIMARY_BUTTONS = 2;  // the "action inputs" tools map onto

}  // namespace board
