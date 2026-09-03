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
  void injectNav(Btn b, BtnEvent e);  // desk-test nav from the serial channel

  // --- display ---
  M5GFX& gfx() { return M5.Display; }
  int w() const { return M5.Display.width(); }
  int h() const { return M5.Display.height(); }

  // brightness 0..100 when awake; persisted to NVS
  void setBrightness(uint8_t pct);
  uint8_t brightness() const { return bright_; }

  // screen sleep: backlight off after `ms` idle (0 = never); persisted.
  // a button press wakes and is swallowed (wakes, doesn't also act).
  void setSleepTimeout(uint32_t ms);
  uint32_t sleepTimeout() const { return sleepMs_; }
  bool awake() const { return awake_; }
  bool wokeThisFrame() const { return woke_; }
  uint32_t idleMs() const { return millis() - lastAct_; }  // since last input
  void poke();  // reset idle timer; wake if asleep

  // --- system ---
  uint32_t nowMs() const { return millis(); }
  const char* stationId();   // "stick-<6 hex of eFuse MAC>", stable per device
  const char* macAddress();  // Wi-Fi STA MAC "aa:bb:cc:dd:ee:ff" (router sees this)
  size_t psramBytes() const { return ESP.getPsramSize(); }
  size_t freeHeap() const { return ESP.getFreeHeap(); }
  int battPercent() const { return M5.Power.getBatteryLevel(); }
  bool onUsb() const { return M5.Power.isCharging(); }

 private:
  static constexpr uint32_t kHoldMs = 500;
  static constexpr uint32_t kDefaultSleepMs = 30000;
  static constexpr uint8_t kDefaultBright = 80;
  struct BtnState {
    bool down = false;
    uint32_t downAt = 0;
    bool holdFired = false;
    BtnEvent pending = BtnEvent::None;
  };
  BtnState a_, b_;
  void poll(BtnState& s, bool raw, uint32_t now);

  // --- screen sleep ---
  uint8_t bright_ = kDefaultBright;      // configured awake brightness, 0..100
  uint32_t sleepMs_ = kDefaultSleepMs;   // idle -> backlight off; 0 = never
  uint32_t lastAct_ = 0;                 // millis() of last input
  bool awake_ = true;
  bool woke_ = false;                    // set for the one frame a wake happens
  void applyBacklight(bool on);
  void loadPrefs();
  void savePrefs();
};

extern Hal hal;
