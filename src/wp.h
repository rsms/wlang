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

#define auto __auto_type

#if __has_c_attribute(fallthrough)
  #define FALLTHROUGH [[fallthrough]]
#else
  #define FALLTHROUGH
#endif

#ifndef DEBUG
  #define DEBUG 0
#endif
#if DEBUG
  #include <stdio.h>
  #define dlog(format, ...) \
    fprintf(stderr, "D " format " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)
  #define logerr(format, ...) \
    fprintf(stderr, format " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)
#else
  #define dlog(...)
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

// -------------------------------------------------------------------------------------

// error
typedef enum {
  OK = 0,
  Error,     // generic error
  ErrSize,   // size is wrong
} Err;

// sds, Str
#include "sds.h"
#define Str  sds
#define CStr constsds

inline static bool strhasprefix(CStr s, const char* prefix) {
  size_t slen = sdslen(s);
  size_t plen = strlen(prefix);
  return slen < plen ? false : memcmp(s, prefix, plen) == 0;
}

inline static Str strgrow(Str s, size_t addlSize) {
  return sdsMakeRoomFor(s, align2(addlSize, 128));
}

// Sym
#include "sym.h"

// Source
typedef struct {
  Str       name;
  const u8* buf;
  size_t    len;       // length of buf
  u32*      _lineoffsets;
  u32       _linecount;
} Source;

// SrcPos
typedef struct {
  Source* src;   // source
  u32     offs;  // offset into src->buf
  u32     span;  // span length. 0 = unknown or does no apply.
} SrcPos;
#define NoSrcPos (({ SrcPos p = {NULL,0,0}; p; }))

// LineCol
typedef struct { u32 line; u32 col; } LineCol;

void SourceInit(Source*, Str name, const u8* buf, size_t len);
void SourceFree(Source*);
Str SrcPosMsg(Str s, SrcPos, Str message);
Str SrcPosFmt(Str s, SrcPos pos); // "<file>:<line>:<col>"
LineCol SrcPosLineCol(SrcPos);

// -----------------------------
// Unicode
typedef i32 Rune;
static const Rune RuneErr  = 0xFFFD; // Unicode replacement character
static const Rune RuneSelf = 0x80;
  // characters below RuneSelf are represented as themselves in a single byte.
static const u32 UTF8Max = 4; // Maximum number of bytes of a UTF8-encoded char.

Rune utf8decode(const u8* buf, size_t len, u32* out_width);

// -----------------------------------------
// scanner
#include "token.h"

// Get printable name
const char* TokName(Tok);

// scanner flags
typedef enum {
  SCAN_DEFAULT  = 1 << 0,
  SCAN_COMMENTS = 1 << 2,  // populate S.comments
} ScanFlags;

// scanned comment
typedef struct Comment {
  struct Comment* next; // next comment in linked list
  Source*         src;  // source
  const u8*       ptr;  // ptr into source
  size_t          len;  // byte length
} Comment;

// scanner
typedef struct S {
  Source*   src;           // input source
  const u8* inp;           // input buffer current pointer
  const u8* inp0;          // input buffer previous pointer
  const u8* inend;         // input buffer end
  ScanFlags flags;

  Tok       tok;           // current token
  const u8* tokstart;      // start of current token
  const u8* tokend;        // end of current token
  Sym       name;          // Current name (valid for TIdent and keywords)
  bool      insertSemi;    // insert a semicolon before next newline
  Comment*  comments;      // linked list head of comments scanned so far
  Comment*  comments_tail; // linked list tail of comments scanned so far

  u32       lineno;     // source position line
  const u8* linestart;  // source position line start pointer (for column)
} S;

void SInit(S*, Source*, ScanFlags);
Tok  SNext(S*);

// source position of current token
inline static SrcPos SSrcPos(S* s) {
  assert(s->tokstart >= s->src->buf);
  assert(s->tokstart < (s->src->buf + s->src->len));
  assert(s->tokend >= s->tokstart);
  assert(s->tokend <= (s->src->buf + s->src->len));
  size_t offs = s->tokstart - s->src->buf;
  SrcPos p = { s->src, offs, s->tokend - s->tokstart };
  return p;
}

// -----------------------------------------------------------
// ast
#include "ast.h"

// parser
typedef struct P {
  S   s;      // scanner
  u32 fnest;  // function nesting level (for error handling)
} P;

void PInit(P*, Source*);
void PParseFile(P*);

// util
u8* readfile(const char* filename, size_t* bufsize_out);
