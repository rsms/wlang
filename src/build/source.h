#pragma once
#include "../common/defs.h"
#include "../common/str.h"

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

// NoSrcPos is the "null" of SrcPos
#define NoSrcPos (({ SrcPos p = {NULL,0,0}; p; }))

// LineCol
typedef struct { u32 line; u32 col; } LineCol;

void SourceInit(Source*, Str name, const u8* buf, size_t len);
void SourceFree(Source*);
Str SrcPosMsg(Str s, SrcPos, ConstStr message);
Str SrcPosFmt(Str s, SrcPos pos); // "<file>:<line>:<col>"
LineCol SrcPosLineCol(SrcPos);
