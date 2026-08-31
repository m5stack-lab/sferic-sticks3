#include "capture_screen.h"

#include "hal.h"
#include "pcap.h"

namespace {
constexpr int kTitleH = 18;
}

void CaptureScreen::tick() {
  const BtnEvent a = hal.event(Btn::A);
  const BtnEvent b = hal.event(Btn::B);
  if (b == BtnEvent::Hold) {
    pop();
    return;
  }
  if (a == BtnEvent::Click) {
    capture.reset();
    dirty_ = true;
  } else if (b == BtnEvent::Click) {
    Serial.printf("[capture] export %u frames %u bytes\n", capture.frames(),
                  (unsigned)capture.used());
    capture.dumpBase64(Serial);
  }
  if (hal.nowMs() - lastDraw_ > 500) dirty_ = true;
  if (dirty_) {
    draw();
    dirty_ = false;
    lastDraw_ = hal.nowMs();
  }
}

void CaptureScreen::draw() {
  auto& g = hal.gfx();
  g.fillScreen(TFT_BLACK);
  g.fillRect(0, 0, hal.w(), kTitleH, TFT_DARKGREY);
  g.setTextColor(TFT_WHITE, TFT_DARKGREY);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Settings  Capture");

  int y = kTitleH + 10;
  g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g.setCursor(6, y);
  g.printf("frames  %u", (unsigned)capture.frames());
  g.setCursor(6, y + 14);
  g.printf("used    %u KB", (unsigned)(capture.used() / 1024));
  g.setCursor(6, y + 28);
  g.printf("cap     %u KB", (unsigned)(capture.capacity() / 1024));
  g.setCursor(6, y + 42);
  g.setTextColor(capture.recording()  ? TFT_GREEN
                 : capture.full()     ? TFT_RED
                                      : TFT_DARKGREY,
                 TFT_BLACK);
  g.print(capture.recording() ? "recording" : capture.full() ? "full" : "idle");

  g.setTextColor(TFT_DARKGREY, TFT_BLACK);
  g.setCursor(4, hal.h() - 9);
  g.print("A clear  B export  B(hold) back");
}
