#pragma once

#include <M5Unified.h>
#include <cstddef>
#include <cstdint>

// Thin hardware layer over M5Unified. Tools and the UI talk to this, not to
// M5.* directly, so the M5 dependency stays in one place.

enum class Btn : uint8_t { A, B };

// A consumed-once input event for the current frame.
enum class BtnEvent : uint8_t {
  None,
  Click,  // short press released
  Hold,   // held past the long-press threshold (fires once)
};

class Hal {
 public:
  void begin();
  void update();  // once per loop: refresh M5, serial sim, edge detection

  // --- input ---
  BtnEvent event(Btn b);  // returns and clears this frame's event for b
  bool held(Btn b) const;

  // --- display ---
  M5GFX& gfx() { return M5.Display; }
  int w() const { return M5.Display.width(); }
  int h() const { return M5.Display.height(); }
  void backlight(uint8_t pct);  // 0..100

  // --- system ---
  uint32_t nowMs() const { return millis(); }
  size_t psramBytes() const { return ESP.getPsramSize(); }
  size_t freeHeap() const { return ESP.getFreeHeap(); }
  int battPercent() const { return M5.Power.getBatteryLevel(); }
  bool onUsb() const { return M5.Power.isCharging(); }

 private:
  static constexpr uint32_t kHoldMs = 500;
  struct BtnState {
    bool down = false;
    uint32_t downAt = 0;
    bool holdFired = false;
    BtnEvent pending = BtnEvent::None;
  };
  BtnState a_, b_;
  void poll(BtnState& s, bool raw, uint32_t now);
  void pollSerial();  // 'a'/'A'/'b'/'B' -> Click/Hold, for on-desk testing
};

extern Hal hal;
