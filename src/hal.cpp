#include "hal.h"

#include <Preferences.h>

#include <cstdio>

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

const char* Hal::stationId() {
  static char id[16] = {0};
  if (!id[0]) {
    const uint64_t mac = ESP.getEfuseMac();
    std::snprintf(id, sizeof(id), "stick-%06lx",
                  (unsigned long)(mac & 0xFFFFFF));
  }
  return id;
}

void Hal::injectNav(Btn b, BtnEvent e) {
  // Desk-test nav forwarded from the serial channel (SerialControl owns the
  // UART). Respects the one-event-per-frame rule and wakes the screen, so
  // 'a'/'b' over serial behaves like a real press.
  BtnState& s = (b == Btn::A) ? a_ : b_;
  if (s.pending == BtnEvent::None) s.pending = e;
  poke();
}

void Hal::update() {
  M5.update();
  woke_ = false;
  const uint32_t now = millis();

  const bool aRaw = M5.BtnA.isPressed();
  const bool bRaw = M5.BtnB.isPressed();

  if (!awake_) {
    // Asleep: the backlight is off but the firmware and any running tool keep
    // ticking. The first physical press wakes the screen and is swallowed here
    // so it doesn't also drive the menu. (Serial nav wakes via injectNav ->
    // poke(), which runs before this in loop().)
    const bool edge = (aRaw && !a_.down) || (bRaw && !b_.down);
    if (edge) {
      poke();  // restores brightness, sets woke_, resets the idle timer
      // Seed button state as already-held so the wake press yields no event.
      a_.down = aRaw; a_.holdFired = aRaw; a_.pending = BtnEvent::None;
      b_.down = bRaw; b_.holdFired = bRaw; b_.pending = BtnEvent::None;
    }
    return;
  }

  poll(a_, aRaw, now);
  poll(b_, bRaw, now);

  // Only a debounced Click/Hold (or a serial nav char injected via injectNav,
  // which also lands as a pending event) counts as activity. Checking the raw
  // level here meant a stuck or noisy button line pinned the screen awake
  // forever; a stuck line now fires at most one Hold, then latches quiet.
  if (a_.pending != BtnEvent::None || b_.pending != BtnEvent::None)
    lastAct_ = now;

  if (sleepMs_ != 0 && (now - lastAct_) >= sleepMs_) {
    awake_ = false;
    applyBacklight(false);
    Serial.printf("[hal] screen sleep (idle %lus)\n",
                  (unsigned long)((now - lastAct_) / 1000));
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
    Serial.println("[hal] screen wake");
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
