#include <Arduino.h>
#include <unity.h>
#include "ScrollBuffer.h"

void setUp(void) {}
void tearDown(void) {}

void test_push_and_size() {
  ScrollBuffer b;
  TEST_ASSERT_EQUAL_UINT(0, b.size());
  b.push("hello");
  b.push("world");
  TEST_ASSERT_EQUAL_UINT(2, b.size());
}

void test_truncation_and_get() {
  ScrollBuffer b;
  b.push("12345678901234567890OK"); // > 20
  char out[ScrollBuffer::kWidth + 1];
  b.get(0, out);
  TEST_ASSERT_EQUAL_UINT(20, strlen(out));
  TEST_ASSERT_EQUAL_CHAR('0', out[19]);
}

void test_ring_wrap() {
  ScrollBuffer b;
  for (int i = 0; i < (int)ScrollBuffer::kCapacity + 5; ++i) {
    char msg[21];
    snprintf(msg, sizeof(msg), "L%02d", i);
    b.push(msg);
  }
  TEST_ASSERT_EQUAL_UINT(ScrollBuffer::kCapacity, b.size());
  char out[ScrollBuffer::kWidth + 1];
  b.get(0, out);
  // After overflow, first should be L05
  TEST_ASSERT_EQUAL_STRING("L05", out);
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_push_and_size);
  RUN_TEST(test_truncation_and_get);
  RUN_TEST(test_ring_wrap);
  UNITY_END();
}

void loop() {}

