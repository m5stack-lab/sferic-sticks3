#include "ui_kit.h"

#include <M5Unified.h>

#include "hal.h"
#include "icons.h"
#include "theme.h"

namespace kit {

int listRows() { return BODY_H / ROW_H; }
int statRows() { return BODY_H / STAT_H; }

void titleBar(const char* text, uint16_t accent) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  g.fillRect(0, 0, W, TITLE_H, th.bgAlt);
  g.fillRect(0, TITLE_H - 2, W, 2, accent);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(6, 6);
  g.print(text);
}

void hintBar(const char* text) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  g.fillRect(0, H - HINT_H, W, HINT_H, th.bg);
  g.setTextColor(th.fgDim, th.bg);
  g.setTextSize(1);
  g.setCursor(4, H - HINT_H + 2);
  g.print(text);
}

void bodyClear() {
  hal.gfx().fillRect(0, BODY_Y, W, BODY_H, theme().bg);
}

void begin(const char* title, uint16_t accent) {
  bodyClear();
  titleBar(title, accent);
}

void end(const char* hint) { hintBar(hint); }

void row(int slot, const char* label, bool selected, const char* iconKey,
         uint16_t iconColor, char dot, uint16_t dotColor) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  const int y = BODY_Y + slot * ROW_H;
  if (selected) g.fillRect(0, y, W, ROW_H, th.selBg);

  int tx = 8;
  if (iconKey && *iconKey) {
    icon::draw(iconKey, 3, y + 2, 20, iconColor ? iconColor : th.fg);
    tx = 30;
  }
  g.setTextColor(selected ? th.selFg : th.fg, selected ? th.selBg : th.bg);
  g.setTextSize(2);
  g.setCursor(tx, y + 4);
  g.print(label);

  if (dot) {
    const int dx = W - 14, dy = y + ROW_H / 2;
    const uint16_t dc = dotColor ? dotColor : th.fgDim;
    if (dot == 'x')
      g.fillCircle(dx, dy, 4, dc);
    else
      g.drawCircle(dx, dy, 4, dc);
  }
}

void stat(int slot, const char* label, const char* value, uint16_t valColor) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  const int y = BODY_Y + 2 + slot * STAT_H;
  g.setTextSize(2);
  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(6, y);
  g.print(label);
  g.setTextColor(valColor ? valColor : th.fg, th.bg);
  g.setCursor(6 + 12 * 8, y);  // ~8 chars of label column
  g.print(value);
}

void bigValue(const char* text, uint16_t color) {
  auto& g = hal.gfx();
  g.setTextSize(3);
  g.setTextColor(color, theme().bg);
  g.setCursor(8, BODY_Y + BODY_H / 2 - 12);
  g.print(text);
}

void spectrum(const float* mag, int n, float norm) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  const int gx = 2, gy = BODY_Y + 4, gw = W - 4, gh = BODY_H - 8;
  if (norm < 1.0f) norm = 1.0f;
  for (int x = 0; x < gw; ++x) {
    const int b = (int)((long)x * (n - 1) / (gw > 1 ? gw : 1));
    int h = (int)(gh * (mag[b] / norm));
    if (h < 0) h = 0;
    if (h > gh) h = gh;
    const uint16_t c =
        h > gh * 2 / 3 ? th.bad : h > gh / 3 ? th.warn : th.ok;
    g.drawFastVLine(gx + x, gy + gh - h, h, c);
  }
}

}  // namespace kit
