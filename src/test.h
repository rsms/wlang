#pragma once
#include "assert.h"

// testing
#if DEBUG
  #define W_UNIT_TEST_ENABLED 1
  #define W_UNIT_TEST(name, body) \
    __attribute__((constructor)) static void unit_test_##name() { \
      if (getTestMode() != WTestModeNone) {                       \
      printf("TEST " #name " %s\n", __FILE__);                    \
      body                                                        \
      }                                                           \
    }
#else
  #define W_UNIT_TEST(name, body)
  #define W_UNIT_TEST_ENABLED 0
#endif

typedef enum WTestMode {
                      // W_TEST_MODE  Description
  WTestModeNone = 0,  // ""           testing disabled
  WTestModeOn,        // "on"         testing enabled
  WTestModeExclusive, // "exclusive"  only test; don't run main function
} WTestMode;

// getTestMode retrieves the effective WTestMode parsed from environment W_TEST_MODE
WTestMode getTestMode();
