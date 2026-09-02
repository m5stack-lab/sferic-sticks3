#include "display_screen.h"

#include <cstdio>

#include "hal.h"
#include "theme.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kRowH = 16;
constexpr int kRows = 3;  // Sleep, Brightness, Theme

const uint32_t kSleepMs[] = {0, 15000, 30000, 60000, 120000, 300000};
const char* kSleepLbl[] = {"Never", "15s", "30s", "1m", "2m", "5m"};
constexpr int kSleepN = sizeof(kSleepMs) / sizeof(kSleepMs[0]);

const uint8_t kBright[] = {20, 40, 60, 80, 100};
constexpr int kBrightN = sizeof(kBright) / sizeof(kBright[0]);

int sleepIdx() {
  const uint32_t cur = hal.sleepTimeout();
  for (int i = 0; i < kSleepN; ++i)
    if (kSleepMs[i] == cur) return i;
  return 2;
}
int brightIdx() {
  const uint8_t cur = hal.brightness();
  for (int i = 0; i < kBrightN; ++i)
    if (kBright[i] == cur) return i;
  return 3;
}
}  // namespace

void DisplayScreen::tick() {
  if (hal.wokeThisFrame()) dirty_ = true;
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);

  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click || a == BtnEvent::Hold) {
    sel_ = (sel_ + 1) % kRows;
    dirty_ = true;
  } else if (b == BtnEvent::Click) {
    if (sel_ == 0)
      hal.setSleepTimeout(kSleepMs[(sleepIdx() + 1) % kSleepN]);
    else if (sel_ == 1)
      hal.setBrightness(kBright[(brightIdx() + 1) % kBrightN]);
    else
      setThemeIndex(themeIndex() + 1);
    Serial.printf("[display] sleep=%s bright=%u%% theme=%s\n",
                  kSleepLbl[sleepIdx()], (unsigned)hal.brightness(),
                  theme().name);
    dirty_ = true;
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void DisplayScreen::draw() {
  auto& g = hal.gfx();
  const Theme& th = theme();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Settings  Display");

  char brightBuf[8];
  std::snprintf(brightBuf, sizeof(brightBuf), "%u%%", (unsigned)hal.brightness());
  const char* names[kRows] = {"Sleep", "Brightness", "Theme"};
  const char* vals[kRows] = {kSleepLbl[sleepIdx()], brightBuf, th.name};

  for (int i = 0; i < kRows; ++i) {
    const int y = kTitleH + i * kRowH;
    const bool onSel = (i == sel_);
    if (onSel) g.fillRect(0, y, hal.w(), kRowH, th.selBg);
    g.setTextColor(onSel ? th.selFg : th.fg, onSel ? th.selBg : th.bg);
    g.setCursor(6, y + 4);
    g.printf("%-11s %s", names[i], vals[i]);
  }

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print("A row  B change  B(hold) back");
}
