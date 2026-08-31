#pragma once

#include <cstdint>

// Two-step "arm, then confirm" guard so a stray button press can't fire an
// action that emits RF/IR or keystrokes. First trigger arms; a second
// trigger within kWindowMs fires; a cancel press or a timeout disarms.
struct Arm {
  static constexpr uint32_t kWindowMs = 3000;
  bool armed = false;
  uint32_t at = 0;

  // Call once per tick. Returns true on the confirming press only.
  bool update(bool trigger, bool cancel, uint32_t now) {
    if (armed && now - at > kWindowMs) armed = false;
    if (cancel && armed) {
      armed = false;
      return false;
    }
    if (trigger) {
      if (!armed) {
        armed = true;
        at = now;
        return false;
      }
      armed = false;
      return true;
    }
    return false;
  }
};
