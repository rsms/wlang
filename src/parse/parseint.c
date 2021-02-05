#include "parseint.h"
#include <limits.h>

#define GEN_STRTO_X(BITS, MAXVAL) \
bool parseint##BITS(const char* pch, size_t size, int base, u##BITS* result) { \
  assert(base >= 2 && base <= 36);                                             \
  const char* s = pch;                                                         \
  const char* end = pch + size;                                                \
  u##BITS acc = 0;                                                             \
  u##BITS cutoff = MAXVAL;                                                     \
  u##BITS cutlim = cutoff % base;                                              \
  cutoff /= base;                                                              \
  int any = 0;                                                                 \
  for (char c = *s; s != end; c = *++s) {                                      \
    if (c >= '0' && c <= '9') {                                                \
      c -= '0';                                                                \
    } else if (c >= 'A' && c <= 'Z') {                                         \
      c -= 'A' - 10;                                                           \
    } else if (c >= 'a' && c <= 'z') {                                         \
      c -= 'a' - 10;                                                           \
    } else {                                                                   \
      return false;                                                            \
    }                                                                          \
    if (c >= base) {                                                           \
      return false;                                                            \
    }                                                                          \
    if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {            \
      any = -1;                                                                \
    } else {                                                                   \
      any = 1;                                                                 \
      acc *= base;                                                             \
      acc += c;                                                                \
    }                                                                          \
  }                                                                            \
  if (any < 0 ||  /* more digits than what fits in acc */                      \
      any == 0)                                                                \
  {                                                                            \
    return false;                                                              \
  }                                                                            \
  *result = acc;                                                               \
  return true;                                                                 \
}

GEN_STRTO_X(32, 0xFFFFFFFFu)
GEN_STRTO_X(64, 0xFFFFFFFFFFFFFFFFull)

#ifndef NDEBUG
__attribute__((constructor)) static void test() {
  #define T32(cstr, base, expectnum) (({ \
    u32 result = 0; \
    bool ok = parseint32(cstr, strlen(cstr), base, &result); \
    assert(ok || !cstr); \
    if (result != expectnum) { fprintf(stderr, "result: 0x%X\n", result); } \
    assert(result == expectnum || !"got: "&& result); \
  }))

  #define T64(cstr, base, expectnum) (({ \
    u64 result = 0; \
    bool ok = parseint64(cstr, strlen(cstr), base, &result); \
    assert(ok || !cstr); \
    if (result != expectnum) { fprintf(stderr, "result: 0x%llX\n", result); } \
    assert(result == expectnum || !"got: "&& result); \
  }))

  T32("FFAA3191", 16, 0xFFAA3191);
  T32("0", 16, 0);
  T32("000000", 16, 0);
  T32("7FFFFFFF", 16, 0x7FFFFFFF);
  T32("EFFFFFFF", 16, 0xEFFFFFFF);
  T32("FFFFFFFF", 16, 0xFFFFFFFF);

  // fits in s64
  T64("7fffffffffffffff",       16, 0x7FFFFFFFFFFFFFFF);
  T64("9223372036854775807",    10, 0x7FFFFFFFFFFFFFFF);
  T64("777777777777777777777",  8,  0x7FFFFFFFFFFFFFFF);
  T64("1y2p0ij32e8e7",          36, 0x7FFFFFFFFFFFFFFF);

  T64("efffffffffffffff",       16, 0xEFFFFFFFFFFFFFFF); // this caught a bug once

  T64("ffffffffffffffff",       16, 0xFFFFFFFFFFFFFFFF);
  T64("18446744073709551615",   10, 0xFFFFFFFFFFFFFFFF);
  T64("1777777777777777777777", 8,  0xFFFFFFFFFFFFFFFF);
  T64("3w5e11264sgsf",          36, 0xFFFFFFFFFFFFFFFF);
}
#endif
