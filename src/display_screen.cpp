#include "display_screen.h"

#include <cstdio>

#include "hal.h"

namespace {
constexpr int kTitleH = 18;
constexpr int kRowH = 16;

const uint32_t kSleepMs[] = {0, 15000, 30000, 60000, 120000, 300000};
const char* kSleepLbl[] = {"Never", "15s", "30s", "1m", "2m", "5m"};
constexpr int kSleepN = sizeof(kSleepMs) / sizeof(kSleepMs[0]);

const uint8_t kBright[] = {20, 40, 60, 80, 100};
constexpr int kBrightN = sizeof(kBright) / sizeof(kBright[0]);

// Index of the entry >= current, so an unrecognised stored value snaps to the
// nearest step on the first press.
int sleepIdx() {
  const uint32_t cur = hal.sleepTimeout();
  for (int i = 0; i < kSleepN; ++i)
    if (kSleepMs[i] == cur) return i;
  return 2;  // default 30s
}
int brightIdx() {
  const uint8_t cur = hal.brightness();
  for (int i = 0; i < kBrightN; ++i)
    if (kBright[i] == cur) return i;
  return 3;  // default 80
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
  if (a == BtnEvent::Click) {
    sel_ = (sel_ + 1) % 2;
    dirty_ = true;
  } else if (a == BtnEvent::Hold) {
    sel_ = (sel_ + 1) % 2;  // only two rows — wrap either way
    dirty_ = true;
  } else if (b == BtnEvent::Click) {
    if (sel_ == 0) {
      hal.setSleepTimeout(kSleepMs[(sleepIdx() + 1) % kSleepN]);
    } else {
      hal.setBrightness(kBright[(brightIdx() + 1) % kBrightN]);
    }
    Serial.printf("[display] sleep=%s bright=%u%%\n", kSleepLbl[sleepIdx()],
                  (unsigned)hal.brightness());
    dirty_ = true;
  }

  if (dirty_) {
    draw();
    dirty_ = false;
  }
}

void DisplayScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Settings  Display");

  const char* vals[2] = {kSleepLbl[sleepIdx()], nullptr};
  char brightBuf[8];
  std::snprintf(brightBuf, sizeof(brightBuf), "%u%%", (unsigned)hal.brightness());
  vals[1] = brightBuf;
  const char* names[2] = {"Sleep", "Brightness"};

  for (int i = 0; i < 2; ++i) {
    const int y = kTitleH + i * kRowH;
    const bool onSel = (i == sel_);
    if (onSel) g.fillRect(0, y, hal.w(), kRowH, TFT_NAVY);
    g.setTextColor(onSel ? TFT_WHITE : TFT_LIGHTGREY, onSel ? TFT_NAVY : TFT_BLACK);
    g.setCursor(6, y + 4);
    g.printf("%-11s %s", names[i], vals[i]);
  }

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A row  B change  B(hold) back");
}
