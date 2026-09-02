#include "capture_screen.h"

#include "hal.h"
#include "theme.h"
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
  const Theme& th = theme();
  auto& g = hal.gfx();
  g.fillScreen(th.bg);
  g.fillRect(0, 0, hal.w(), kTitleH, th.bgAlt);
  g.setTextColor(th.fg, th.bgAlt);
  g.setTextSize(1);
  g.setCursor(4, 5);
  g.print("Settings  Capture");

  int y = kTitleH + 10;
  g.setTextColor(th.fg, th.bg);
  g.setCursor(6, y);
  g.printf("frames  %u", (unsigned)capture.frames());
  g.setCursor(6, y + 14);
  g.printf("used    %u KB", (unsigned)(capture.used() / 1024));
  g.setCursor(6, y + 28);
  g.printf("cap     %u KB", (unsigned)(capture.capacity() / 1024));
  g.setCursor(6, y + 42);
  g.setTextColor(capture.recording()  ? th.ok
                 : capture.full()     ? th.bad
                                      : th.fgDim,
                 th.bg);
  g.print(capture.recording() ? "recording" : capture.full() ? "full" : "idle");

  g.setTextColor(th.fgDim, th.bg);
  g.setCursor(4, hal.h() - 9);
  g.print("A clear  B export  B(hold) back");
}
