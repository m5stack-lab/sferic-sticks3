// Host unit tests for the hardware-independent logic. Runs under the
// `native` env: `pio test -e native`. Anything that touches M5Unified /
// Arduino stays out of here.

#include <unity.h>

#include "../src/confirm.h"
#include "../src/toolset_label.h"

// ---------------- Arm (two-step arm/confirm guard) ----------------

void test_arm_needs_two_triggers(void) {
  Arm a;
  TEST_ASSERT_FALSE(a.update(/*trigger=*/true, /*cancel=*/false, 0));  // arms
  TEST_ASSERT_TRUE(a.armed);
  TEST_ASSERT_TRUE(a.update(/*trigger=*/true, /*cancel=*/false, 100));  // fires
  TEST_ASSERT_FALSE(a.armed);  // and disarms
}

void test_arm_idle_tick_does_nothing(void) {
  Arm a;
  TEST_ASSERT_FALSE(a.update(false, false, 0));
  TEST_ASSERT_FALSE(a.armed);
}

void test_arm_cancel_disarms(void) {
  Arm a;
  a.update(true, false, 0);  // arm
  TEST_ASSERT_FALSE(a.update(/*trigger=*/false, /*cancel=*/true, 50));
  TEST_ASSERT_FALSE(a.armed);
  // a following trigger only re-arms, does not fire
  TEST_ASSERT_FALSE(a.update(true, false, 60));
  TEST_ASSERT_TRUE(a.armed);
}

void test_arm_times_out(void) {
  Arm a;
  a.update(true, false, 1000);  // arm at t=1000
  // trigger after the window: treated as a fresh arm, not a confirm
  TEST_ASSERT_FALSE(a.update(true, false, 1000 + Arm::kWindowMs + 1));
  TEST_ASSERT_TRUE(a.armed);
}

void test_arm_confirm_at_window_edge_still_fires(void) {
  Arm a;
  a.update(true, false, 0);
  // exactly kWindowMs later — `now - at > kWindowMs` is false, still armed
  TEST_ASSERT_TRUE(a.update(true, false, Arm::kWindowMs));
}

void test_arm_cancel_when_not_armed_is_noop(void) {
  Arm a;
  TEST_ASSERT_FALSE(a.update(false, true, 0));
  TEST_ASSERT_FALSE(a.armed);
}

// ---------------- toolsetLabel ----------------

void test_toolset_label_known(void) {
  TEST_ASSERT_EQUAL_STRING("WiFi", toolsetLabel("wifi"));
  TEST_ASSERT_EQUAL_STRING("Bluetooth", toolsetLabel("ble"));
  TEST_ASSERT_EQUAL_STRING("Infrared", toolsetLabel("ir"));
  TEST_ASSERT_EQUAL_STRING("USB", toolsetLabel("usb"));
  TEST_ASSERT_EQUAL_STRING("Sensors", toolsetLabel("sensors"));
  TEST_ASSERT_EQUAL_STRING("SubGHz", toolsetLabel("subghz"));
  TEST_ASSERT_EQUAL_STRING("2.4GHz", toolsetLabel("nrf24"));
}

void test_toolset_label_unknown_passthrough(void) {
  TEST_ASSERT_EQUAL_STRING("mystery", toolsetLabel("mystery"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_arm_needs_two_triggers);
  RUN_TEST(test_arm_idle_tick_does_nothing);
  RUN_TEST(test_arm_cancel_disarms);
  RUN_TEST(test_arm_times_out);
  RUN_TEST(test_arm_confirm_at_window_edge_still_fires);
  RUN_TEST(test_arm_cancel_when_not_armed_is_noop);
  RUN_TEST(test_toolset_label_known);
  RUN_TEST(test_toolset_label_unknown_passthrough);
  return UNITY_END();
}
