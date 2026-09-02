#include "ui_kit.h"

#include <M5Unified.h>

#include <cstdio>
#include <cstring>

#include "hal.h"
#include "icons.h"
#include "theme.h"

namespace kit {

int listRows() { return BODY_H / ROW_H; }
int statRows() { return BODY_H / STAT_H; }

namespace {
// A bold chevron. dir -1 points left (tip on the left), +1 points right.
// Drawn 2px thick, arms `half` tall spreading `reach` from the tip.
void bigChevron(int ax, int ay, int half, int reach, int dir, uint16_t c) {
  auto& g = hal.gfx();
  const int tip = ax + dir * reach;  // -1 -> left of ax, +1 -> right of ax
  for (int o = 0; o < 2; ++o) {
    g.drawLine(ax + o, ay - half, tip + o, ay, c);
    g.drawLine(tip + o, ay, ax + o, ay + half, c);
  }
}
}  // namespace

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

void statusBar() {
  auto& g = hal.gfx();
  const Theme& th = theme();
  g.fillRect(0, 0, W, STATUS_H, th.bgAlt);
  g.fillRect(0, STATUS_H - 2, W, 2, th.accentSubghz);  // violet rule
  g.setTextSize(1);

  const uint32_t s = hal.nowMs() / 1000;
  char clk[12];
  std::snprintf(clk, sizeof(clk), "%lu:%02lu:%02lu", (unsigned long)(s / 3600),
                (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  g.setTextColor(th.fg, th.bgAlt);
  g.setCursor(4, 5);
  g.print(clk);

  char pwr[10];
  std::snprintf(pwr, sizeof(pwr), "%s%d%%", hal.onUsb() ? "= " : "", hal.battPercent());
  const int pw = (int)std::strlen(pwr) * 6;
  g.setTextColor(hal.battPercent() <= 15 ? th.bad : th.fg, th.bgAlt);
  g.setCursor(W - pw - 4, 5);
  g.print(pwr);
}

void carousel(const char* iconKey, uint16_t accent, const char* label) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  const int top = STATUS_H;
  const int bot = H - HINT_H;
  g.fillRect(0, top, W, bot - top, th.bg);

  const int iconS = 64;
  const int iconY = top + 6;
  if (iconKey && *iconKey) icon::draw(iconKey, W / 2 - iconS / 2, iconY, iconS, accent);

  const int n = (int)std::strlen(label);
  int sz = 3;  // shrink to fit: long tool names drop to size 2 / 1
  if (n * 18 > W - 16) sz = 2;
  if (sz == 2 && n * 12 > W - 12) sz = 1;
  g.setTextSize(sz);
  g.setTextColor(th.fg, th.bg);
  const int lw = n * 6 * sz;
  g.setCursor((W - lw) / 2, iconY + iconS + (sz == 3 ? 4 : 10));
  g.print(label);

  const int cy = iconY + iconS / 2;
  bigChevron(14, cy, 14, 9, -1, th.fgDim);
  bigChevron(W - 14, cy, 14, 9, +1, th.fgDim);
}

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

namespace {
struct MqSlot {
  uint32_t key = 0;    // FNV hash of the current text
  int off = 0;         // px scrolled
  int dir = 1;
  uint32_t at = 0;     // last step ms
  bool moving = false;
};
MqSlot g_mq[4];

uint32_t fnv(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ (uint8_t)*s) * 16777619u;
  return h;
}
}  // namespace

void marquee(const char* text, int x, int y, int boxW, int size, uint16_t color,
             int id) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  if (id < 0 || id > 3) id = 0;
  MqSlot& m = g_mq[id];

  const uint32_t k = fnv(text);
  if (k != m.key) {
    m.key = k;
    m.off = 0;
    m.dir = 1;
    m.at = hal.nowMs();
    m.moving = false;
  }

  const int textW = (int)strlen(text) * 6 * size;
  const int over = textW - boxW;
  if (over > 0) {
    const uint32_t now = hal.nowMs();
    if (now - m.at > 45) {
      m.at = now;
      // pause ~700ms at each end
      const bool atEnd = (m.dir > 0 && m.off >= over) || (m.dir < 0 && m.off <= 0);
      if (!atEnd) m.off += m.dir * 3;
      else if (now - m.at > 700) m.dir = -m.dir;
      if (m.off < 0) m.off = 0;
      if (m.off > over) m.off = over;
      m.moving = !((m.dir > 0 && m.off >= over) || (m.dir < 0 && m.off <= 0));
    }
  } else {
    m.moving = false;
  }

  g.setClipRect(x, y - 1, boxW, size * 8 + 2);
  g.setTextSize(size);
  g.setTextColor(color, th.bg);
  g.setCursor(x - (over > 0 ? m.off : 0), y);
  g.print(text);
  g.clearClipRect();
}

bool marqueeAnimating() {
  for (auto& m : g_mq)
    if (m.moving) return true;
  return false;
}

void detail(const char* headline, const char* subline, int n, int total) {
  auto& g = hal.gfx();
  const Theme& th = theme();
  const int top = TITLE_H;
  const int bot = H - HINT_H;
  g.fillRect(0, top, W, bot - top, th.bg);

  // same < > paging affordance as the menu carousel
  const int cy = (top + bot) / 2 - 8;
  bigChevron(12, cy, 12, 8, -1, th.fgDim);
  bigChevron(W - 12, cy, 12, 8, +1, th.fgDim);

  marquee(headline, 26, cy - 6, W - 52, 2, th.fg, 0);

  g.setTextSize(1);
  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(26, cy + 16);
  g.print(subline);

  char c[16];
  std::snprintf(c, sizeof(c), "%d/%d", n, total);
  g.setTextColor(th.accentSubghz, th.bg);
  g.setCursor(W - (int)strlen(c) * 6 - 14, cy + 16);
  g.print(c);
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

void headline(const char* text, uint16_t color) {
  auto& g = hal.gfx();
  g.setTextSize(3);
  g.setTextColor(color ? color : theme().fg, theme().bg);
  g.setCursor(8, BODY_Y + 8);
  g.print(text);
}

void line(int slot, const char* text, uint16_t color) {
  auto& g = hal.gfx();
  g.setTextSize(2);
  g.setTextColor(color ? color : theme().fg, theme().bg);
  g.setCursor(8, BODY_Y + 40 + slot * 22);
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
