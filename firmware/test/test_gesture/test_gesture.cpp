#include <unity.h>
#include "GestureRecognizer.h"

using mm::ButtonEvent;
using mm::GestureRecognizer;

// Helper: feed a stable level until the debounce window passes, return last event.
static ButtonEvent settle(GestureRecognizer& g, bool pressed, uint32_t& t) {
  ButtonEvent ev = g.update(pressed, t);  // edge candidate starts here
  t += 25;                                // > debounce (20ms)
  ButtonEvent ev2 = g.update(pressed, t); // edge commits here
  return (ev2 != ButtonEvent::None) ? ev2 : ev;
}

void test_short_press(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  TEST_ASSERT_EQUAL(ButtonEvent::None, settle(g, true, t));   // press
  t += 200;                                                   // hold 200ms < long
  TEST_ASSERT_EQUAL(ButtonEvent::Short, settle(g, false, t)); // release -> SHORT
}

void test_long_press(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  settle(g, true, t);                                         // press
  t += 700;                                                   // hold 700ms >= long, < verylong
  TEST_ASSERT_EQUAL(ButtonEvent::Long, settle(g, false, t));  // release -> LONG
}

void test_very_long_latches_while_held(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  settle(g, true, t);                                         // press
  t += 3100;                                                  // exceed verylong (3000ms)
  TEST_ASSERT_EQUAL(ButtonEvent::VeryLong, g.update(true, t));// fires while still held
  t += 500;
  TEST_ASSERT_EQUAL(ButtonEvent::None, settle(g, false, t));  // release after latch -> nothing
}

void test_glitch_shorter_than_debounce_ignored(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(true, t));    // bounce up
  t += 5;
  TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(false, t));   // bounce down before 20ms
  t += 50;
  TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(false, t));   // no event emitted
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_short_press);
  RUN_TEST(test_long_press);
  RUN_TEST(test_very_long_latches_while_held);
  RUN_TEST(test_glitch_shorter_than_debounce_ignored);
  return UNITY_END();
}
