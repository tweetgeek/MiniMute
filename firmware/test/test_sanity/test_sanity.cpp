#include <unity.h>

void test_toolchain_runs(void) {
  TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_toolchain_runs);
  return UNITY_END();
}
