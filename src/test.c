#include <stdlib.h>
#include <string.h>
// #include <execinfo.h>

#include "defs.h"
#include "test.h"

static WTestMode _testMode = (WTestMode)-1;

WTestMode getTestMode() {
  if (_testMode == (WTestMode)-1) {
    _testMode = WTestModeNone;
    char* testmode = getenv("W_TEST_MODE");
    if (testmode != NULL) {
      if (strcmp(testmode, "on") == 0) {
        _testMode = WTestModeOn;
      } else if (strcmp(testmode, "exclusive") == 0) {
        _testMode = WTestModeExclusive;
      }
    }
  }
  return _testMode;
}
