#include "../wp.h"
#include "ir.h"


IRValue* IRValueNew(IRFun* f, IRBlock* b, IROp op, TypeCode type, const SrcPos* pos) {
  assert(f->vid < 0xFFFFFFFF); // too many block IDs generated
  auto v = (IRValue*)memalloc(f->mem, sizeof(IRValue));
  v->id = f->vid++;
  v->op = op;
  v->type = type;
  if (pos != NULL) {
    v->pos = *pos;
  }
  if (b != NULL) {
    ArrayPush(&b->values, v, b->f->mem);
  } else {
    dlog("WARN IRValueNew b=NULL");
  }
  return v;
}

void IRValueAddComment(IRValue* v, Memory mem, ConstStr comment) {
  if (comment != NULL) { // allow passing NULL to do nothing
    auto commentLen = sdslen(comment);
    if (commentLen > 0) {
      if (v->comment == NULL) {
        v->comment = memallocCStr(mem, comment, commentLen);
      } else {
        v->comment = memallocCStrConcat(mem, v->comment, "; ", comment, NULL);
      }
    }
  }
}
