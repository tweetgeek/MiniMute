#include <unity.h>
#include "AppLinkProtocol.h"
#include "minimute_protocol.h"

using namespace mm;

void test_encode_button_event(void) {
  uint8_t b = 0xFF;
  TEST_ASSERT_TRUE(encodeButtonEvent(ButtonEvent::Short, b));  TEST_ASSERT_EQUAL_UINT8(MM_BTN_SHORT, b);
  TEST_ASSERT_TRUE(encodeButtonEvent(ButtonEvent::Long, b));   TEST_ASSERT_EQUAL_UINT8(MM_BTN_LONG, b);
  TEST_ASSERT_TRUE(encodeButtonEvent(ButtonEvent::Double, b)); TEST_ASSERT_EQUAL_UINT8(MM_BTN_DOUBLE, b);
}

void test_very_long_and_none_not_sent(void) {
  uint8_t b = 0;
  TEST_ASSERT_FALSE(encodeButtonEvent(ButtonEvent::VeryLong, b));
  TEST_ASSERT_FALSE(encodeButtonEvent(ButtonEvent::None, b));
}

void test_decode_indicator(void) {
  IndicatorState s = decodeIndicator(MM_IND_MUTED | MM_IND_IN_CALL);
  TEST_ASSERT_TRUE(s.muted);
  TEST_ASSERT_TRUE(s.inCall);
  TEST_ASSERT_FALSE(s.cameraOff);
}

void test_encode_status(void) {
  uint8_t s = encodeStatus(/*charging*/true, /*low*/false, /*power*/1);
  TEST_ASSERT_TRUE(s & MM_ST_CHARGING);
  TEST_ASSERT_FALSE(s & MM_ST_LOW_BATTERY);
  TEST_ASSERT_EQUAL_UINT8(1, (s & MM_ST_POWER_MASK) >> MM_ST_POWER_SHIFT);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_encode_button_event);
  RUN_TEST(test_very_long_and_none_not_sent);
  RUN_TEST(test_decode_indicator);
  RUN_TEST(test_encode_status);
  return UNITY_END();
}
