#include "wp.h"
#include "typeid.h"


static const i64 minIntVal[TypeCode_INTRINSIC_NUM_END] = {
  0,                   // bool
  -0x80,               // int8
  0,                   // uint8
  -0x8000,             // int16
  0,                   // uint16
  -0x80000000,         // int32
  0,                   // uint32
  -0x8000000000000000, // int64
  0,                   // uint64
  0,                   // TODO float32
  0,                   // TODO float64
};

static const u64 maxIntVal[TypeCode_INTRINSIC_NUM_END] = {
  1,                   // bool
  0x7f,                // int8
  0xff,                // uint8
  0x7fff,              // int16
  0xffff,              // uint16
  0x7fffffff,          // int32
  0xffffffff,          // uint32
  0x7fffffffffffffff,  // int64
  0xffffffffffffffff,  // uint64
  0,                   // TODO float32
  0,                   // TODO float64
};

// convert an intrinsic numeric value v to an integer of type tc
static bool convvalToInt(CCtx* cc, Node* srcnode, NVal* v, TypeCode tc) {
  assert(tc < TypeCode_INTRINSIC_NUM_END);
  if (TypeCodeIsInt(tc)) {
    // int -> int; check overflow and simply leave as-is (reinterpret.)
    if ((i64)v->i < minIntVal[tc] || maxIntVal[tc] < v->i) {
      CCtxErrorf(cc, srcnode->pos, "constant %s overflows %s", NValStr(v), TypeCodeName(tc));
      // TODO: consider actually truncating v->i. Probably not useful as IR builder does that.
    }
    return true;
  }
  dlog("TODO convvalToInt tc=%s", TypeCodeName(tc));
  return false;
}

// convert an intrinsic numeric value v to a floating point number of type tc
static bool convvalToFloat(CCtx* cc, Node* srcnode, NVal* v, TypeCode t) {
  dlog("TODO convvalToFloat");
  return false;
}


// convval converts v into a representation appropriate for t.
// If no such representation exists, return false.
static bool convval(CCtx* cc, Node* srcnode, NVal* v, Node* t, bool explicit) {
  switch (t->kind) {

  case NBasicType: {
    auto tc = t->t.basic.typeCode;
    if (TypeCodeIsInt(tc)) {
      if (tc >= TypeCode_INTRINSIC_NUM_END) {
        assert(tc == TypeCode_int || tc == TypeCode_uint);
        tc = (tc == TypeCode_int) ? TypeCode_int32 : TypeCode_uint32;
      }
      return convvalToInt(cc, srcnode, v, tc);
    } else if (TypeCodeIsFloat(tc)) {
      return convvalToFloat(cc, srcnode, v, tc);
    }
  }
  break;

  default:
    dlog("TODO convval t->kind %s", NodeKindName(t->kind));
    break;
  }
  // return (NVal){TypeCode_nil,{0}};
  return false;
}


// convlit converts an expression to type t.
// If expr is already of type t, expr is simply returned.
//
// For explicit conversions, t must be non-nil.
// For implicit conversions (e.g., assignments), t may be nil;
// if so, expr is converted to its default type.
//
Node* _convlit(CCtx* cc, Node* expr, Node* t, bool explicit) {
  assert(t != NULL);
  assert(NodeKindIsType(t->kind));
  dlog("convlit %s as %s", NodeReprShort(expr), NodeReprShort(t));

  if (expr->type != NULL) {
    dlog("no-op -- expr is already typed: %s", NodeReprShort(expr->type));
    return expr;
  }
  // if (expr->type != NULL && TypeEquals(expr->type, t)) {
  //   // expr is already of target type
  //   return expr;
  // }

  switch (expr->kind) {
  case NIntLit:
    dlog("  NIntLit");
    if (convval(cc, expr, &expr->val, t, explicit)) {
      expr->type = t;
      return expr;
    }
    break;

  default:
    dlog("  TODO expr->kind %s", NodeKindName(expr->kind));
    break;
  }

  return NULL;
}



