#include "hal.h"

Hal hal;

void Hal::begin() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);  // landscape 240x135
  M5.Display.setTextWrap(false);
  M5.Display.fillScreen(TFT_BLACK);
}

void Hal::poll(BtnState& s, bool raw, uint32_t now) {
  if (raw && !s.down) {
    s.down = true;
    s.downAt = now;
    s.holdFired = false;
  } else if (raw && s.down && !s.holdFired && (now - s.downAt) >= kHoldMs) {
    s.holdFired = true;
    s.pending = BtnEvent::Hold;
  } else if (!raw && s.down) {
    s.down = false;
    if (!s.holdFired) s.pending = BtnEvent::Click;
  }
}

void Hal::pollSerial() {
  // One nav char per frame, and only once the last event has been consumed,
  // so a burst of chars produces one press each instead of collapsing.
  if (!Serial.available()) return;
  if (a_.pending != BtnEvent::None || b_.pending != BtnEvent::None) return;
  switch (Serial.read()) {
    case 'a': a_.pending = BtnEvent::Click; break;
    case 'A': a_.pending = BtnEvent::Hold; break;
    case 'b': b_.pending = BtnEvent::Click; break;
    case 'B': b_.pending = BtnEvent::Hold; break;
    default: break;
  }
}

void Hal::update() {
  M5.update();
  const uint32_t now = millis();
  poll(a_, M5.BtnA.isPressed(), now);
  poll(b_, M5.BtnB.isPressed(), now);
  pollSerial();
}

BtnEvent Hal::event(Btn b) {
  BtnState& s = (b == Btn::A) ? a_ : b_;
  const BtnEvent e = s.pending;
  s.pending = BtnEvent::None;
  return e;
}

bool Hal::held(Btn b) const { return (b == Btn::A) ? a_.down : b_.down; }

void Hal::backlight(uint8_t pct) {
  if (pct > 100) pct = 100;
  M5.Display.setBrightness((uint8_t)(pct * 255 / 100));
}
