#include "icons.h"

#include <M5Unified.h>
#include <math.h>

#include <cstring>

#include "hal.h"

namespace {

// A chevron pointing `up` (or right when horizontal=true), apex at (ax, ay),
// arms of length ~half spreading to the opening side.
void chevron(int ax, int ay, int half, int drop, uint16_t c, bool up = true) {
  auto& g = hal.gfx();
  if (up) {
    g.drawLine(ax - half, ay + drop, ax, ay, c);
    g.drawLine(ax, ay, ax + half, ay + drop, c);
  } else {  // pointing right
    g.drawLine(ax - drop, ay - half, ax, ay, c);
    g.drawLine(ax, ay, ax - drop, ay + half, c);
  }
}

}  // namespace

namespace icon {

void wifi(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int cx = x + s / 2, base = y + s - 3;
  chevron(cx, base - s * 5 / 8, s * 7 / 16, s * 5 / 16, c);
  chevron(cx, base - s * 3 / 8, s * 5 / 16, s / 4, c);
  chevron(cx, base - s / 8, s * 3 / 16, s / 8, c);
  g.fillCircle(cx, base, 1 + s / 24, c);
}

void ble(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int cx = x + s / 2, t = y + 2, b = y + s - 2, m = (t + b) / 2;
  const int w = s / 4;
  g.drawLine(cx, t, cx, b, c);
  g.drawLine(cx, t, cx + w, y + s / 4, c);
  g.drawLine(cx + w, y + s / 4, cx - w, y + s * 3 / 4, c);
  g.drawLine(cx - w, y + s * 3 / 4, cx, b, c);
  g.drawLine(cx, m, cx + w, y + s * 3 / 4, c);
  g.drawLine(cx, m, cx - w, y + s / 4, c);
}

void ir(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int ly = y + s / 2;
  g.fillRect(x + 1, ly - s / 6, s / 5, s / 3, c);  // emitter
  const int ax = x + s / 3;
  chevron(ax + s / 6, ly, s / 3, s / 4, c, false);
  chevron(ax + s / 3, ly, s / 3, s / 4, c, false);
  chevron(ax + s / 2, ly, s / 3, s / 4, c, false);
}

void sensors(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();  // a pulse / ECG mark
  const int y0 = y + s / 2, x0 = x + 2, x1 = x + s - 2;
  g.drawLine(x0, y0, x0 + s / 3, y0, c);
  g.drawLine(x0 + s / 3, y0, x + s / 2, y + 2, c);
  g.drawLine(x + s / 2, y + 2, x + s * 2 / 3, y + s - 2, c);
  g.drawLine(x + s * 2 / 3, y + s - 2, x + s * 3 / 4, y0, c);
  g.drawLine(x + s * 3 / 4, y0, x1, y0, c);
}

void subghz(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int cx = x + s / 3;
  g.drawLine(cx, y + 2, cx, y + s - 2, c);          // antenna
  g.drawLine(cx - s / 6, y + s - 2, cx + s / 6, y + s - 2, c);  // base
  chevron(x + s * 2 / 3, y + s / 3, s / 3, s / 4, c, false);
  chevron(x + s - 3, y + s / 3, s / 3, s / 4, c, false);
}

void nrf24(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int cx = x + s / 2;
  g.fillCircle(cx, y + s / 2, 1 + s / 20, c);
  chevron(cx - s / 4, y + s / 2, s / 3, s / 4, c, false);            // left
  g.drawLine(cx + s / 4 - s / 4, y + s / 2, cx + s / 4, y + s / 2 - s / 3, c);
  chevron(cx + s / 4 + s / 4, y + s / 2, s / 3, s / 4, c, false);    // right-ish
  g.drawLine(cx, y + 3, cx, y + s / 2, c);  // little mast
}

void gear(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int cx = x + s / 2, cy = y + s / 2, r = s * 3 / 8;
  g.drawCircle(cx, cy, r, c);
  g.drawCircle(cx, cy, r / 2, c);
  for (int k = 0; k < 6; ++k) {
    const float a = k * 3.14159f / 3.0f;
    const int px = cx + (int)((r + 2) * cosf(a));
    const int py = cy + (int)((r + 2) * sinf(a));
    g.fillRect(px - 1, py - 1, 3, 3, c);
  }
}

void globe(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int cx = x + s / 2, cy = y + s / 2, r = s * 3 / 8;
  g.drawCircle(cx, cy, r, c);
  g.drawLine(cx, cy - r, cx, cy + r, c);
  g.drawLine(cx - r, cy, cx + r, cy, c);
  g.drawEllipse(cx, cy, r / 2, r, c);
  g.drawLine(cx - r + 2, cy - r / 2, cx + r - 2, cy - r / 2, c);
  g.drawLine(cx - r + 2, cy + r / 2, cx + r - 2, cy + r / 2, c);
}

void info(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int cx = x + s / 2, cy = y + s / 2, r = s * 3 / 8;
  g.drawCircle(cx, cy, r, c);
  g.fillCircle(cx, cy - r / 2, 1 + s / 24, c);
  g.fillRect(cx - 1, cy - r / 6, 2 + s / 24, r / 2 + r / 6, c);
}

void save(int x, int y, int s, uint16_t c) {
  auto& g = hal.gfx();
  const int x0 = x + 2, y0 = y + 2, w = s - 4, h = s - 4;
  g.drawRect(x0, y0, w, h, c);
  g.fillRect(x0 + w / 4, y0, w / 2, h / 3, c);           // label
  g.drawRect(x0 + w / 4, y0 + h / 2, w / 2, h / 2 - 2, c);  // shutter
}

void draw(const char* key, int x, int y, int s, uint16_t c) {
  if (!key || !*key) return;
  if (!std::strcmp(key, "wifi")) return wifi(x, y, s, c);
  if (!std::strcmp(key, "ble")) return ble(x, y, s, c);
  if (!std::strcmp(key, "ir")) return ir(x, y, s, c);
  if (!std::strcmp(key, "sensors")) return sensors(x, y, s, c);
  if (!std::strcmp(key, "subghz")) return subghz(x, y, s, c);
  if (!std::strcmp(key, "nrf24")) return nrf24(x, y, s, c);
  if (!std::strcmp(key, "gear")) return gear(x, y, s, c);
  if (!std::strcmp(key, "globe")) return globe(x, y, s, c);
  if (!std::strcmp(key, "info")) return info(x, y, s, c);
  if (!std::strcmp(key, "save")) return save(x, y, s, c);
}

}  // namespace icon
