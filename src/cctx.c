// CCtx compilation context
#include "wp.h"

// reset and/or initialize a compilation context
void CCtxInit(
  CCtx*         cc,
  ErrorHandler* errh,
  void*         userdata,
  Str           srcname,
  const u8*     srcbuf,
  size_t        srclen
) {
  if (cc->src.buf != NULL) {
    free((void*)cc->src.buf);
    cc->src.buf = NULL;
  }
  if (cc->src.name != NULL) {
    SourceFree(&cc->src);
  }
  SourceInit(&cc->src, srcname, srcbuf, srclen);
  FWAllocInit(&cc->mem);
  cc->errh = errh;
  cc->userdata = userdata;
}


void CCtxFree(CCtx* cc) {
  if (cc->src.buf != NULL) {
    free((void*)cc->src.buf);
    cc->src.buf = NULL;
  }
  SourceFree(&cc->src);
  FWAllocFree(&cc->mem);
}
