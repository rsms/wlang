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
    fprintf(stderr, "D " format "\t(%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)
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

// testing
#if DEBUG
  #define W_UNIT_TEST(name, body) \
    __attribute__((constructor)) static void unit_test_##name() { \
      dlog("TEST " #name); \
      do body while(0); \
    }

// #define W_UNIT_TEST(name, body) \
//     __attribute__((constructor)) static void unit_test_##name() \
//     body
#else
  #define W_UNIT_TEST(name, body)
#endif

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


// -------------------------------------------------------------------------------------
// memory
#include "memory.h"

// shadow stdlib functions so that if we try to use them we get an error
#define malloc()  abort() /* use memalloc instead */
#define calloc()  abort() /* use memalloc instead */
#define realloc() abort() /* use memrealloc instead */
#define free()    abort() /* use memfree instead */

// ZERO zeroes memory of a thing. e.g: Foo foo; ZERO(foo);
#define ZERO(stackthing) memset(&(stackthing), 0, sizeof(stackthing))

// -------------------------------------------------------------------------------------

// error
typedef enum {
  OK = 0,
  Error,     // generic error
  ErrSize,   // size is wrong
} Err;



// sds, Str
#include "sds.h"
#define Str      sds
#define ConstStr constsds

inline static bool strhasprefix(ConstStr s, const char* prefix) {
  size_t slen = sdslen(s);
  size_t plen = strlen(prefix);
  return slen < plen ? false : memcmp(s, prefix, plen) == 0;
}

inline static Str strgrow(Str s, size_t addlSize) {
  return sdsMakeRoomFor(s, align2(addlSize, 128));
}

// strrepr returns a printable representation of an sds string (sds, Sym, etc.)
// using sdscatrepr which encodes non-printable ASCII chars for safe printing.
// E.g. "foo\x00bar" if the string contains a zero byte.
// Not thread safe! Use sdscatrepr when you need thread safety.
const char* strrepr(ConstStr);


// TmpData allocates "size" number of bytes to be used for a short amount of time.
// The returned memory is valid until the next call to TmpRecycle() in the same thread.
void* TmpData(size_t size);

// TmpRecycle frees all memory allocated with TmpData.
void TmpRecycle();


// Sym
#include "sym.h"


// Source
typedef struct {
  Str       name;
  const u8* buf;       // owned by caller
  size_t    len;       // length of buf
  u32*      _lineoffsets;
  u32       _linecount;
} Source;

// SrcPos
// TODO: considering implementing something like lico and Pos/XPos from go
//       https://golang.org/src/cmd/internal/src/pos.go
//       https://golang.org/src/cmd/internal/src/xpos.go
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
Str SrcPosMsg(Str s, SrcPos, ConstStr message);
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

typedef void(ErrorHandler)(const Source*, SrcPos, ConstStr msg, void* userdata);

// -----------------------------------------
// scanner
#include "token.h"

// parser & scanner flags
typedef enum {
  ParseFlagsDefault = 0,
  ParseComments     = 1 << 1, // parse comments, populating S.comments
  ParseOpt          = 1 << 2, // apply optimizations. might produce a non-1:1 AST/token stream
} ParseFlags;

// scanned comment
typedef struct Comment {
  struct Comment* next; // next comment in linked list
  Source*         src;  // source
  const u8*       ptr;  // ptr into source
  size_t          len;  // byte length
} Comment;

// scanner
typedef struct S {
  Memory     mem;
  Source*    src;          // input source
  const u8*  inp;          // input buffer current pointer
  const u8*  inp0;         // input buffer previous pointer
  const u8*  inend;        // input buffer end
  ParseFlags flags;

  Tok       tok;           // current token
  const u8* tokstart;      // start of current token
  const u8* tokend;        // end of current token
  Sym       name;          // Current name (valid for TIdent and keywords)
  bool      insertSemi;    // insert a semicolon before next newline
  Comment*  comments;      // linked list head of comments scanned so far
  Comment*  comments_tail; // linked list tail of comments scanned so far

  u32       lineno;     // source position line
  const u8* linestart;  // source position line start pointer (for column)

  ErrorHandler* errh;
  void*         userdata;
} S;

void SInit(S*, Memory, Source*, ParseFlags, ErrorHandler*, void* userdata);
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
// parsing and compiling
#include "ast.h"
#include "array.h"

// compilation context
typedef struct {
  ErrorHandler* errh;
  void*         userdata; // passed to errh
  Source        src;
  Memory        mem; // memory used only during compilation, like AST nodes
} CCtx;

// initialize and/or recycle a CCtx
void CCtxInit(
  CCtx*,
  ErrorHandler* errh,
  void*         userdata,
  Str           srcname,
  const u8*     srcbuf,  // caller owns
  size_t        srclen
);
void CCtxFree(CCtx*);
void CCtxErrorf(const CCtx* cc, SrcPos pos, const char* format, ...);

// parser
typedef struct P {
  S      s;          // scanner
  u32    fnest;      // function nesting level (for error handling)
  u32    unresolved; // number of unresolved identifiers
  Scope* scope;      // current scope
  CCtx*  cc;         // compilation context
} P;
Node* Parse(P*, CCtx*, ParseFlags, Scope* pkgscope);
Node* NodeOptIfCond(Node* n); // TODO: move this and parser into a parse.h file

// Symbol resolver
Node* ResolveSym(CCtx*, ParseFlags, Node*, Scope*);

// Type resolver
void ResolveType(CCtx*, Node*);


// os
size_t os_mempagesize();  // always returns a suitable number
u8* os_readfile(const char* filename, size_t* bufsize_out, Memory mem/*null*/);
