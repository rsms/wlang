#pragma once
#include "assert.h"
//
// testing
//
// Preprocessor macros:
//   W_TEST_BUILD is defined for the "test" target product (but not for "debug".)
//   W_UNIT_TEST_ENABLED is defined for "test" and "debug" targets (since DEBUG is.)
//   W_UNIT_TEST(name, body) defines a unit test to be run before main()
//

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
