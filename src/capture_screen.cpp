#include "capture_screen.h"

#include <cstdio>

#include "hal.h"
#include "theme.h"
#include "ui_kit.h"
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
  kit::begin("Capture", th.fg);

  const bool rec = capture.recording();
  kit::headline(rec ? "REC" : capture.full() ? "FULL" : "IDLE",
                rec ? th.ok : capture.full() ? th.bad : th.fgDim);

  char l[40];
  std::snprintf(l, sizeof(l), "frames  %u", (unsigned)capture.frames());
  kit::line(0, l);
  std::snprintf(l, sizeof(l), "%u / %u KB", (unsigned)(capture.used() / 1024),
               (unsigned)(capture.capacity() / 1024));
  kit::line(1, l, th.fgDim);

  kit::end("A clear   B export   B- back");
}
