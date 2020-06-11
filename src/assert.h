#pragma once
#include <assert.h>
//
// assertion testing
//
// assert((bool)cond)
//   prints error and calls abort() if cond is false
//   debug builds: active
//   opt build:    eliminated
//
// assertf((bool)cond, const char* format, ...)
//   prints format with arguments and calls abort() if cond is false.
//   debug builds: active
//   opt build:    eliminated
//
// checknull<T>(T expr) -> T
//   evaluates expr and if the result is null, calls assert(result). Returns result.
//   debug builds: active
//   opt build:    pass-through
//

// WAssertf prints message with format, including a stack trace if available.
// If srcfile != NULL, attempts to print source code around srcline.
void WAssertf(const char* nullable srcfile, int srcline, const char* nonull format, ...);

#ifdef assert
  #undef assert
#endif

#ifdef DEBUG
  #define assertf(cond, format, ...)                                                     \
    ({ if (!(cond)) {                                                                    \
      WAssertf(__FILE__, __LINE__, "%s:%d: " format, __FILE__, __LINE__, ##__VA_ARGS__); \
      abort();                                                                           \
    } })

  #define assert(cond)                                                      \
    ({ if (!(cond)) {                                                       \
      WAssertf(__FILE__, __LINE__, "%s:%d: %s", __FILE__, __LINE__, #cond); \
      abort();                                                              \
    } })

  const char* _assert_joinstr(const char* s1, ... /* NULL terminated */);

  #define asserteq(expr, expect)                                           \
    ({ auto actual = (expr);                                               \
       auto expected = (expect);                                           \
      if (actual != expected) {                                            \
        WAssertf(__FILE__, __LINE__,                                       \
          _assert_joinstr("%s:%d: %s ; got ", WFormatForValue(actual),     \
                          ", expected ", WFormatForValue(expected), NULL), \
          __FILE__, __LINE__, #expr, actual, expected);                    \
        abort();                                                           \
      }                                                                    \
    })

  #define checknull(expr) \
    ({ auto v = (expr); assert(v != NULL); v; })

#else
  #define assertf(cond, format, ...) do{}while(0)
  #define assert(cond, ...)          do{}while(0)
  #define asserteq(expr, expect)     do{}while(0)
  #define checknull(expr)            expr
#endif

