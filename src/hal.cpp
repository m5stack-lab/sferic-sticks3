#include "hal.h"

#include <Preferences.h>

Hal hal;

void Hal::begin() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);  // landscape 240x135
  M5.Display.setTextWrap(false);
  M5.Display.fillScreen(TFT_BLACK);
  loadPrefs();
  awake_ = true;
  woke_ = false;
  lastAct_ = millis();
  applyBacklight(true);
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
  woke_ = false;
  const uint32_t now = millis();

  const bool aRaw = M5.BtnA.isPressed();
  const bool bRaw = M5.BtnB.isPressed();

  if (!awake_) {
    // Asleep: the backlight is off but the firmware and any running tool keep
    // ticking. The first input just wakes the screen and is swallowed here so
    // it doesn't also drive the menu.
    const bool edge = (aRaw && !a_.down) || (bRaw && !b_.down) || Serial.available();
    if (edge) {
      while (Serial.available()) Serial.read();
      poke();  // restores brightness, sets woke_, resets the idle timer
      // Seed button state as already-held so the wake press yields no event.
      a_.down = aRaw; a_.holdFired = aRaw; a_.pending = BtnEvent::None;
      b_.down = bRaw; b_.holdFired = bRaw; b_.pending = BtnEvent::None;
    }
    return;
  }

  poll(a_, aRaw, now);
  poll(b_, bRaw, now);
  pollSerial();

  if (aRaw || bRaw || a_.pending != BtnEvent::None || b_.pending != BtnEvent::None)
    lastAct_ = now;

  if (sleepMs_ != 0 && (now - lastAct_) >= sleepMs_) {
    awake_ = false;
    applyBacklight(false);
  }
}

BtnEvent Hal::event(Btn b) {
  BtnState& s = (b == Btn::A) ? a_ : b_;
  const BtnEvent e = s.pending;
  s.pending = BtnEvent::None;
  return e;
}

bool Hal::held(Btn b) const { return (b == Btn::A) ? a_.down : b_.down; }

void Hal::poke() {
  lastAct_ = millis();
  if (!awake_) {
    awake_ = true;
    woke_ = true;
    applyBacklight(true);
  }
}

void Hal::setBrightness(uint8_t pct) {
  if (pct > 100) pct = 100;
  bright_ = pct;
  if (awake_) applyBacklight(true);
  savePrefs();
}

void Hal::setSleepTimeout(uint32_t ms) {
  sleepMs_ = ms;
  lastAct_ = millis();
  savePrefs();
}

void Hal::applyBacklight(bool on) {
  const uint8_t pct = on ? bright_ : 0;
  M5.Display.setBrightness((uint8_t)(pct * 255 / 100));
}

void Hal::loadPrefs() {
  Preferences p;
  if (p.begin("sferic", true)) {
    bright_ = p.getUChar("disp.bright", kDefaultBright);
    sleepMs_ = p.getULong("disp.sleepms", kDefaultSleepMs);
    p.end();
  }
  if (bright_ > 100) bright_ = kDefaultBright;
}

void Hal::savePrefs() {
  Preferences p;
  if (p.begin("sferic", false)) {
    p.putUChar("disp.bright", bright_);
    p.putULong("disp.sleepms", sleepMs_);
    p.end();
  }
}
