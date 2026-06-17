#include <unity.h>
#include "BatteryGauge.h"

using mm::BatteryGauge;

void test_percent_endpoints(void) {
  BatteryGauge g;
  g.addSample(4200);
  TEST_ASSERT_EQUAL_UINT8(100, g.percent());
  BatteryGauge g2;
  g2.addSample(3250);                    // below table min -> clamp 0
  TEST_ASSERT_EQUAL_UINT8(0, g2.percent());
  BatteryGauge g3;
  g3.addSample(4500);                    // above max -> clamp 100
  TEST_ASSERT_EQUAL_UINT8(100, g3.percent());
}

void test_percent_midpoint_interpolates(void) {
  BatteryGauge g;
  g.addSample(3750);                     // between 3700(45%) and 3800(60%) -> ~52%
  uint8_t p = g.percent();
  TEST_ASSERT_TRUE(p >= 50 && p <= 55);
}

void test_ema_smooths(void) {
  BatteryGauge g;                        // default alpha 0.2
  g.addSample(4000);                     // seeds smoothed = 4000
  g.addSample(3000);                     // spike down
  uint16_t mv = g.smoothedMilliVolts();
  TEST_ASSERT_TRUE(mv > 3700 && mv < 3900);  // 0.2*3000 + 0.8*4000 = 3800
}

void test_low_battery_hysteresis(void) {
  BatteryGauge g;                        // lowEnter 3500, lowExit 3650
  g.addSample(4000);
  TEST_ASSERT_FALSE(g.lowBattery());
  for (int i = 0; i < 20; ++i) g.addSample(3400);   // settle below enter
  TEST_ASSERT_TRUE(g.lowBattery());
  for (int i = 0; i < 5; ++i) g.addSample(3600);    // between exit and enter
  TEST_ASSERT_TRUE(g.lowBattery());                 // stays low (hysteresis)
  for (int i = 0; i < 20; ++i) g.addSample(3700);   // above exit
  TEST_ASSERT_FALSE(g.lowBattery());
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_percent_endpoints);
  RUN_TEST(test_percent_midpoint_interpolates);
  RUN_TEST(test_ema_smooths);
  RUN_TEST(test_low_battery_hysteresis);
  return UNITY_END();
}
