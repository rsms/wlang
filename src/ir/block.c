#include "../wp.h"
#include "ir.h"


IRBlock* IRBlockNew(IRFun* f, IRBlockKind kind, const SrcPos* pos/*?*/) {
  assert(f->bid < 0xFFFFFFFF); // too many block IDs generated
  auto b = (IRBlock*)memalloc(f->mem, sizeof(IRBlock));
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

void IRBlockSetControl(IRBlock* b, IRValue* v) {
  if (b->control) {
    b->control->uses--;
  }
  b->control = v;
  if (v) {
    v->uses++;
  }
}


static void IRBlockAddPred(IRBlock* b, IRBlock* pred) {
  assert(!b->sealed); // cannot modify preds after block is sealed
  // pick first available hole in fixed-size array:
  for (u32 i = 0; i < countof(b->preds); i++) {
    if (b->preds[i] == NULL) {
      b->preds[i] = pred;
      return;
    }
  }
  assert(0 && "trying to add more than countof(IRBlock.preds) blocks");
}

static void IRBlockAddSucc(IRBlock* b, IRBlock* succ) {
  // pick first available hole in fixed-size array:
  for (u32 i = 0; i < countof(b->succs); i++) {
    if (b->succs[i] == NULL) {
      b->succs[i] = succ;
      return;
    }
  }
  assert(0 && "trying to add more than countof(IRBlock.succs) blocks");
}

void IRBlockAddEdgeTo(IRBlock* b1, IRBlock* b2) {
  assert(!b1->sealed); // cannot modify preds after block is sealed
  IRBlockAddSucc(b1, b2); // b1 -> b2
  IRBlockAddPred(b2, b1); // b2 <- b1
  assert(b1->f != NULL);
  assert(b1->f == b2->f); // blocks must be part of the same function
  IRFunInvalidateCFG(b1->f);
}


void IRBlockSetPred(IRBlock* b, u32 index, IRBlock* pred) {
  assert(!b->sealed);
  assert(index < countof(b->preds));
  b->preds[index] = pred;
  assert(b->f != NULL);
  IRFunInvalidateCFG(b->f);
}

void IRBlockDelPred(IRBlock* b, u32 index) {
  assert(!b->sealed);
  assert(index < countof(b->preds));
  if (b->preds[index] != NULL) {
    b->preds[index] = NULL;
    assert(b->f != NULL);
    IRFunInvalidateCFG(b->f);
  }
}


void IRBlockSetSucc(IRBlock* b, u32 index, IRBlock* succ) {
  assert(index < countof(b->succs));
  b->succs[index] = succ;
  assert(b->f != NULL);
  IRFunInvalidateCFG(b->f);
}

void IRBlockDelSucc(IRBlock* b, u32 index) {
  assert(index < countof(b->succs));
  if (b->succs[index] != NULL) {
    b->succs[index] = NULL;
    assert(b->f != NULL);
    IRFunInvalidateCFG(b->f);
  }
}

