#include <unity.h>
#include "PowerManager.h"

using namespace mm;

static PowerManager::Config fastCfg() {
  PowerManager::Config c;
  c.activeTimeoutMs       = 1000;
  c.deepSleepTimeoutMs    = 5000;
  c.disconnectedTimeoutMs = 2000;
  return c;
}

void test_starts_active(void) {
  PowerManager pm(fastCfg());
  TEST_ASSERT_EQUAL(PowerState::Active, pm.update(0));
}

void test_connected_idle_after_timeout(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.noteActivity(0);
  TEST_ASSERT_EQUAL(PowerState::Active, pm.update(500));
  TEST_ASSERT_EQUAL(PowerState::ConnectedIdle, pm.update(1500));  // > activeTimeout
}

void test_activity_wakes_from_idle(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.update(1500);                       // ConnectedIdle
  pm.noteActivity(1600);
  TEST_ASSERT_EQUAL(PowerState::Active, pm.update(1600));
}

void test_deep_sleep_after_long_inactivity(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.noteActivity(0);
  TEST_ASSERT_EQUAL(PowerState::DeepSleep, pm.update(6000));  // > deepSleepTimeout
}

void test_in_call_blocks_deep_sleep(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.noteActivity(0);
  pm.setInCall(true);
  TEST_ASSERT_EQUAL(PowerState::ConnectedIdle, pm.update(6000));  // not DeepSleep
}

void test_disconnected_deep_sleep(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(false, 0);
  TEST_ASSERT_EQUAL(PowerState::DeepSleep, pm.update(2500));  // > disconnectedTimeout
}

void test_disconnected_but_in_call_no_deep_sleep(void) {
  PowerManager pm(fastCfg());
  pm.setInCall(true);
  pm.setConnected(false, 0);
  TEST_ASSERT_NOT_EQUAL(PowerState::DeepSleep, pm.update(2500));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_starts_active);
  RUN_TEST(test_connected_idle_after_timeout);
  RUN_TEST(test_activity_wakes_from_idle);
  RUN_TEST(test_deep_sleep_after_long_inactivity);
  RUN_TEST(test_in_call_blocks_deep_sleep);
  RUN_TEST(test_disconnected_deep_sleep);
  RUN_TEST(test_disconnected_but_in_call_no_deep_sleep);
  return UNITY_END();
}
