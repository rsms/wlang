#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

// target endianess
#if !defined(W_BYTE_ORDER_LE) && !defined(W_BYTE_ORDER_BE)
  #if (defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)) || \
       (defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)) || \
       defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
       defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)
    #define W_BYTE_ORDER_BE 1
  #elif (defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)) || \
         (defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)) || \
         defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
         defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
    #define W_BYTE_ORDER_LE 1
  #else
    #if defined(__i386) || defined(__i386__) || defined(_M_IX86) || \
        defined(__x86_64__) || defined(__x86_64) || \
        defined(__arm__) || defined(__arm) || defined(__ARM__) || \
        defined(__ARM) || defined(__arm64__)
      #define W_BYTE_ORDER_LE 1
    #else
      #error "can't infer endianess. Define W_BYTE_ORDER_LE or W_BYTE_ORDER_BE manually."
    #endif
  #endif
#endif

typedef _Bool                  bool;
typedef signed char            i8;
typedef unsigned char          u8;
typedef signed short int       i16;
typedef unsigned short int     u16;
typedef signed int             i32;
typedef unsigned int           u32;
typedef signed long long int   i64;
typedef unsigned long long int u64;
typedef float                  f32;
typedef double                 f64;

#ifndef true
#define true  ((bool)(1))
#define false ((bool)(0))
#endif

#ifndef null
#define null NULL
#endif

#define nonull   _Nonnull  /* note: nonull conflicts with attribute name */
#define nullable _Nullable

#define auto __auto_type

#if __has_c_attribute(returns_nonnull)
  #define nonull_return __attribute__((returns_nonnull))
#else
  #define nonull_return
#endif

#if __has_c_attribute(fallthrough)
  #define FALLTHROUGH [[fallthrough]]
#else
  #define FALLTHROUGH
#endif

#ifdef DEBUG
  // so that we can simply do: "#if DEBUG"
  #undef DEBUG
  #define DEBUG 1
#elif
  #define DEBUG 0
#endif
#if DEBUG
  #include <stdio.h>
  #define dlog(format, ...) \
    fprintf(stdout, "D " format "\t(%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)
  #define logerr(format, ...) \
    fprintf(stderr, format " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)
#else
  #define dlog(...)  do{}while(0)
  #define logerr(format, ...) \
    fprintf(stderr, format "\n", ##__VA_ARGS__)
#endif

#define max(a,b) \
  ({__typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define min(a,b) \
  ({__typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define countof(x) \
  ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// popcount<T>
#define popcount(x) _Generic((x), \
  unsigned long long: __builtin_popcountll, \
  unsigned long:      __builtin_popcountl, \
  default:            __builtin_popcount \
)(x)

// division of integer, rounding up
#define W_IDIV_CEIL(x, y) (1 + (((x) - 1) / (y)))

#define die(format, ...) do { \
  logerr(format, ##__VA_ARGS__); \
  exit(1); \
} while(0)

// align2 rounds up n to closest boundary w (w must be a power of two)
//
// E.g.
//   align(0, 4) => 0
//   align(1, 4) => 4
//   align(2, 4) => 4
//   align(3, 4) => 4
//   align(4, 4) => 4
//   align(5, 4) => 8
//   ...
//
inline static size_t align2(size_t n, size_t w) {
  assert((w & (w - 1)) == 0); // alignment w is not a power of two
  return (n + (w - 1)) & ~(w - 1);
}

// // Attribute for opting out of address sanitation.
// // Needed for realloc() with a null pointer.
// // e.g.
// // W_NO_SANITIZE_ADDRESS
// // void ThisFunctionWillNotBeInstrumented() { return realloc(NULL, 1); }
// #if defined(__clang__) || defined (__GNUC__)
//   #define W_NO_SANITIZE_ADDRESS __attribute__((no_sanitize("address")))
// #else
//   #define W_NO_SANITIZE_ADDRESS
// #endif
