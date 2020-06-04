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
  // Disabled since srcbuf is owned by caller
  // if (cc->src.buf != NULL) {
  //   free((void*)cc->src.buf);
  //   cc->src.buf = NULL;
  // }
  if (cc->src.name != NULL) {
    SourceFree(&cc->src);
  }
  SourceInit(&cc->src, srcname, srcbuf, srclen);
  cc->mem = MemoryNew(0);
  cc->errh = errh;
  cc->userdata = userdata;
}


void CCtxFree(CCtx* cc) {
  // if (cc->src.buf != NULL) {
  //   free((void*)cc->src.buf);
  //   cc->src.buf = NULL;
  // }
  SourceFree(&cc->src);
  MemoryFree(cc->mem);
}


void CCtxErrorf(CCtx* cc, SrcPos pos, const char* format, ...) {
  if (cc->errh == NULL) {
    return;
  }
  va_list ap;
  va_start(ap, format);
  auto msg = sdsempty();
  if (strlen(format) > 0) {
    msg = sdscatvprintf(msg, format, ap);
    assert(sdslen(msg) > 0); // format may contain %S which is not supported by sdscatvprintf
  }
  va_end(ap);
  cc->errh(&cc->src, pos, msg, cc->userdata);
  sdsfree(msg);
}
