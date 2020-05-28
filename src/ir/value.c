#include "../wp.h"
#include "ir.h"

IRValue* IRValueNew(IRFun* f, IROp op, TypeCode type, const SrcPos*/*nullable*/ pos) {
  assert(f->vid < 0xFFFFFFFF); // too many block IDs generated
  auto v = (IRValue*)FWAlloc(f->mem, sizeof(IRValue));
  v->id = f->vid++;
  v->type = type;
  if (pos != NULL) {
    v->pos = *pos;
  }
  return v;
}

void IRValueAddComment(IRValue* v, FWAllocator* a, ConstStr comment) {
  if (comment != NULL) {
    if (v->comment == NULL) {
      v->comment = FWAllocCStr(a, comment, sdslen(comment));
    } else {
      v->comment = FWAllocCStrConcat(a, v->comment, "; ", comment, NULL);
    }
  }
}
