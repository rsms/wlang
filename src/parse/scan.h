#pragma once
#include "../common/defs.h"
#include "../build/build.h"
#include "../sym.h"
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

// SInit initializes a scanner
void SInit(S*, Memory, Source*, ParseFlags, ErrorHandler*, void* userdata);

// SNext scans the next token
Tok SNext(S*);

// SSrcPos returns the source position of current token
inline static SrcPos SSrcPos(S* s) {
  assert(s->tokstart >= s->src->buf);
  assert(s->tokstart < (s->src->buf + s->src->len));
  assert(s->tokend >= s->tokstart);
  assert(s->tokend <= (s->src->buf + s->src->len));
  size_t offs = s->tokstart - s->src->buf;
  SrcPos p = { s->src, offs, s->tokend - s->tokstart };
  return p;
}
