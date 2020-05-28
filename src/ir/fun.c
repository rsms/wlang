#include "../wp.h"
#include "ir.h"


IRFun* IRFunNew(FWAllocator* a, Node* n) {
  auto f = (IRFun*)FWAlloc(a, sizeof(IRFun));
  f->mem = a;
  ArrayInitWithStorage(&f->blocks, f->blocksStorage, sizeof(f->blocksStorage)/sizeof(void*));
  f->type = n->type;
  assert(f->type != NULL);
  assert(f->type->kind == NFunType);
  f->name = n->fun.name; // may be NULL
  f->pos = n->pos; // copy
  auto params = f->type->fun.params;
  f->nargs = params == NULL ? 0 : params->kind == NTuple ? params->array.a.len : 1;
  return f;
}


// static IRValue* getConst64(IRFun* f, TypeCode t, u64 value) {
//   void* addHint = NULL;
//   auto v = IRConstCacheGet(f->consts, f->mem, t, value, &addHint);
//   if (!v) {
//     dlog("TODO IRFunGetConstInt create const IRValue");
//     v = NULL;
//     f->consts = IRConstCacheAdd(f->consts, f->mem, t, value, v, addHint);
//   }
//   return v;
// }


// OpConstBool,
// OpConstI8,
// OpConstI16,
// OpConstI32,
// OpConstI64,
// OpConstF32,
// OpConstF64,


static IROp ConstTypeCodeToIROp(TypeCode t) {
  switch (t) {
    case TypeCode_bool:
      return OpConstBool;

    case TypeCode_int8:
    case TypeCode_uint8:
      return OpConstI8;

    case TypeCode_int16:
    case TypeCode_uint16:
      return OpConstI16;

    case TypeCode_int:
    case TypeCode_uint:
    case TypeCode_int32:
    case TypeCode_uint32:
      return OpConstI32;

    case TypeCode_int64:
    case TypeCode_uint64:
      return OpConstI64;

    case TypeCode_float32:
      return OpConstF32;

    case TypeCode_float64:
      return OpConstF64;

    case TypeCode_str:
    case TypeCode_nil:
    case TypeCode_fun:
    case TypeCode_tuple:
    case TypeCode_tupleEnd:
    case TypeCode_list:
    case TypeCode_listEnd:
    case TypeCode_struct:
    case TypeCode_structEnd:
    case TypeCode_NUM_END:
    case TypeCode_MAX:
      return OpInvalid;
  }
}


// returns a constant IRValue representing n for type t
IRValue* IRFunGetConstInt(IRFun* f, TypeCode t, u64 value) {
  int addHint = 0;
  auto v = IRConstCacheGet(f->consts, f->mem, t, value, &addHint);
  if (!v) {
    auto op = ConstTypeCodeToIROp(t);
    assert(op != OpInvalid);
    v = IRValueNew(f, op, t, /*SrcPos*/NULL);
    f->consts = IRConstCacheAdd(f->consts, f->mem, t, value, v, addHint);
  }
  return v;
}

IRValue* IRFunGetConstFloat(IRFun* f, TypeCode t, double n) {
  return NULL; // TODO
}




