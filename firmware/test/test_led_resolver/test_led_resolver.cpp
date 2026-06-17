#include <unity.h>
#include "LedStateResolver.h"

using namespace mm;

static LedPattern P(LedColor c, LedMode m) { return LedPattern{c, m}; }

void test_deep_sleep_off(void) {
  LedInputs in; in.deepSleep = true; in.connected = false;
  TEST_ASSERT_TRUE(P(LedColor::Off, LedMode::Solid) == resolveLed(in));
}

void test_searching_blue_slow_blink(void) {
  LedInputs in; in.connected = false;
  TEST_ASSERT_TRUE(P(LedColor::Blue, LedMode::SlowBlink) == resolveLed(in));
}

void test_in_call_muted_red(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = true;
  TEST_ASSERT_TRUE(P(LedColor::Red, LedMode::Solid) == resolveLed(in));
}

void test_in_call_live_green(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = false;
  TEST_ASSERT_TRUE(P(LedColor::Green, LedMode::Solid) == resolveLed(in));
}

void test_charging_amber_breathe(void) {
  LedInputs in; in.connected = true; in.inCall = false; in.charging = true;
  TEST_ASSERT_TRUE(P(LedColor::Amber, LedMode::Breathe) == resolveLed(in));
}

void test_idle_off(void) {
  LedInputs in; in.connected = true; in.inCall = false;
  TEST_ASSERT_TRUE(P(LedColor::Off, LedMode::Solid) == resolveLed(in));
}

void test_identify_overrides_everything(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = true; in.identify = true;
  TEST_ASSERT_TRUE(P(LedColor::White, LedMode::FastBlink) == resolveLed(in));
}

void test_low_battery_flash_overrides_base(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = true; in.lowBatteryFlash = true;
  TEST_ASSERT_TRUE(P(LedColor::Red, LedMode::Solid) == resolveLed(in));  // flash window
}

void test_pairing_outranks_low_battery_flash(void) {
  LedInputs in; in.connected = false; in.lowBatteryFlash = true;
  TEST_ASSERT_TRUE(P(LedColor::Blue, LedMode::SlowBlink) == resolveLed(in));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_deep_sleep_off);
  RUN_TEST(test_searching_blue_slow_blink);
  RUN_TEST(test_in_call_muted_red);
  RUN_TEST(test_in_call_live_green);
  RUN_TEST(test_charging_amber_breathe);
  RUN_TEST(test_idle_off);
  RUN_TEST(test_identify_overrides_everything);
  RUN_TEST(test_low_battery_flash_overrides_base);
  RUN_TEST(test_pairing_outranks_low_battery_flash);
  return UNITY_END();
}
