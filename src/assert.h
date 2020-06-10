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

  inline static const char* _assert_join3(const char* left, const char* mid, const char* right) {
    static char buf[64] = {0};
    char* p = buf;
    memcpy(p, left,  strlen(left)); p += strlen(left);
    memcpy(p, mid,   strlen(mid)); p += strlen(mid);
    memcpy(p, right, strlen(right)); p += strlen(right);
    assert((buf + sizeof(buf)) > p);
    *p = '\0';
    return buf;
  }

  #define asserteq(expr, expect)                                                     \
    ({ auto actual = (expr);                                                         \
      if (actual != (expect)) {                                                      \
        WAssertf(__FILE__, __LINE__,                                                 \
          _assert_join3("%s:%d: %s ; got ", WFormatForValue(expr), ", expected %s"), \
          __FILE__, __LINE__, #expr, actual, #expect);                               \
        abort();                                                                     \
      }                                                                              \
    })

  #define checknull(expr) \
    ({ auto v = (expr); assert(v != NULL); v; })

#else
  #define assertf(cond, format, ...) do{}while(0)
  #define assert(cond, ...)          do{}while(0)
  #define asserteq(expr, expect)     do{}while(0)
  #define checknull(expr)            expr
#endif

