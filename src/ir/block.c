#include "../wp.h"
#include "ir.h"


IRBlock* IRBlockNew(IRFun* f, IRBlockKind kind, const SrcPos* pos/*?*/) {
  assert(f->bid < 0xFFFFFFFF); // too many block IDs generated
  auto b = (IRBlock*)FWAlloc(f->mem, sizeof(IRBlock));
  b->f = f;
  b->id = f->bid++;
  b->kind = kind;
  if (pos != NULL) {
    b->pos = *pos;
  }
  ArrayInitWithStorage(&b->values, b->valuesStorage, sizeof(b->valuesStorage)/sizeof(void*));
  ArrayPush(&f->blocks, b, b->f->mem);
  return b;
}


void IRBlockAddValue(IRBlock* b, IRValue* v) {
  ArrayPush(&b->values, v, b->f->mem);
}
