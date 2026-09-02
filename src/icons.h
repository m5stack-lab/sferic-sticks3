#pragma once

#include <cstdint>

// Procedural glyphs — geometric marks drawn with primitives, no bitmaps, so
// they stay crisp at any size and re-tint per theme for free (ADR-0011 §4).
// Each draws inside an s x s box at (x, y) in `color`.
namespace icon {

void wifi(int x, int y, int s, uint16_t color);
void ble(int x, int y, int s, uint16_t color);
void ir(int x, int y, int s, uint16_t color);
void sensors(int x, int y, int s, uint16_t color);
void subghz(int x, int y, int s, uint16_t color);
void nrf24(int x, int y, int s, uint16_t color);
void gear(int x, int y, int s, uint16_t color);
void globe(int x, int y, int s, uint16_t color);
void info(int x, int y, int s, uint16_t color);
void save(int x, int y, int s, uint16_t color);

// Dispatch by a key: a toolset id ("wifi"|"ble"|"ir"|"sensors"|"subghz"|
// "nrf24") or a UI key ("gear"|"globe"|"info"|"save"). Unknown -> nothing.
void draw(const char* key, int x, int y, int s, uint16_t color);

}  // namespace icon
