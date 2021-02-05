#pragma once
#include "../common/defs.h"
#include "../common/memory.h"
#include "source.h"

// ErrorHandler callback type
typedef void(ErrorHandler)(const Source*, SrcPos, ConstStr msg, void* userdata);

// CCtx compilation context
//
// TODO: Rename to "Build" ("the build")
//
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
