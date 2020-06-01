#include "../wp.h"
#include "ir.h"


IRFun* IRFunNew(Memory mem, Node* n) {
  assert(n->type != NULL);
  assert(n->type->kind == NFunType);
  auto f = (IRFun*)memalloc(mem, sizeof(IRFun));
  f->mem = mem;
  ArrayInitWithStorage(&f->blocks, f->blocksStorage, sizeof(f->blocksStorage)/sizeof(void*));
  f->typeid = n->type->t.id;
  f->name = n->fun.name; // may be NULL
  f->pos = n->pos; // copy
  auto params = n->type->fun.params;
  f->nargs = params == NULL ? 0 : params->kind == NTuple ? params->array.a.len : 1;
  return f;
}


static IRValue* getConst64(IRFun* f, TypeCode t, u64 value) {
  // dlog("getConst64 t=%s value=%llX", TypeCodeName(t), value);
  int addHint = 0;
  auto v = IRConstCacheGet(f->consts, f->mem, t, value, &addHint);
  if (v == NULL) {
    auto op = IROpConstFromAST(t);
    assert(IROpInfo(op)->aux != IRAuxNone);
    // Create const operation and add it to the entry block of function f
    v = IRValueNew(f, f->blocks.v[0], op, t, /*SrcPos*/NULL);
    v->auxInt = value;
    f->consts = IRConstCacheAdd(f->consts, f->mem, t, value, v, addHint);
    // dlog("getConst64 add new const op=%s value=%llX => v%u", IROpNames[op], value, v->id);
  } else {
    // dlog("getConst64 use cached const op=%s value=%llX => v%u", IROpNames[v->op], value, v->id);
  }
  return v;
}

// returns a constant IRValue representing n for type t
IRValue* IRFunGetConstBool(IRFun* f, bool value) {
  // TODO: as there are just two values; avoid using the const cache.
  return getConst64(f, TypeCode_bool, value ? 1 : 0);
}

// returns a constant IRValue representing n for type t
IRValue* IRFunGetConstInt(IRFun* f, TypeCode t, u64 value) {
  assert(TypeCodeIsInt(t));
  return getConst64(f, t, value);
}

IRValue* IRFunGetConstFloat(IRFun* f, TypeCode t, double value) {
  assert(TypeCodeIsFloat(t));
  // reintrepret bits (double is IEEE 754 in C11)
  u64 ivalue = *(u64*)(&value);
  return getConst64(f, t, ivalue);
}
