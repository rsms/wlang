#pragma once

#include "defs.h"
#include "assert.h"
#include "test.h"
#include "memory.h"
// // shadow stdlib functions so that if we try to use them we get an error
// #define malloc()  abort() /* use memalloc instead */
// #define calloc()  abort() /* use memalloc instead */
// #define realloc() abort() /* use memrealloc instead */
// #define free()    abort() /* use memfree instead */
#include "str.h"
#include "sym.h"
#include "os.h"

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
