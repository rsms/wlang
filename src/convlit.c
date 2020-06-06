#include "wp.h"
#include "typeid.h"
#include "ir/op.h"


static const i64 minIntVal[TypeCode_INTRINSIC_NUM_END] = {
  (i64)0,                   // bool
  (i8)-0x80,               // int8
  (i64)0,                   // uint8
  (i16)-0x8000,             // int16
  (i64)0,                   // uint16
  (i32)-0x80000000,         // int32
  (i64)0,                   // uint32
  (i64)-0x8000000000000000, // int64
  (i64)0,                   // uint64
  (i64)0,                   // TODO float32
  (i64)0,                   // TODO float64
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
  // TODO use explicit for allowing "bigger" conversions like for example int -> str.
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
  return false;
}


// convlit converts an expression n to type t.
// If n is already of type t, n is simply returned.
Node* _convlit(CCtx* cc, Node* n, Node* t, bool explicit) {
  assert(t != NULL);
  assert(NodeKindIsType(t->kind));
  dlog("[convlit] %s as %s", NodeReprShort(n), NodeReprShort(t));

  if (n->type != NULL) {
    // already typed
    if (TypeEquals(n->type, t)) {
      return n;
    }
    if (!explicit) {
      dlog("[convlit] no-op -- n is already typed: %s", NodeReprShort(n->type));
      return n;
    }
  }

  switch (n->kind) {

  case NIntLit:
    if (n->type != NULL) {
      n = NodeCopy(cc->mem, n);
    }
    if (convval(cc, n, &n->val, t, explicit)) {
      n->type = t;
      return n;
    }
    break;

  case NBinOp:
    if (t->kind == NBasicType) {
      auto tc = t->t.basic.typeCode;
      // check to see if there is an operation on t; if the type cast is valid
      if (IROpFromAST(n->op.op, tc, tc) == OpNil) {
        break;
      }
      auto left2  = _convlit(cc, n->op.left, t, /* explicit */ false);
      auto right2 = _convlit(cc, n->op.right, t, /* explicit */ false);
      if (left2 == NULL || right2 == NULL) {
        break;
      }
      n->op.left = left2;
      n->op.right = right2;
      if (!TypeEquals(n->op.left->type, n->op.right->type)) {
        CCtxErrorf(cc, n->pos,
          "invalid operation: %s (mismatched types %s and %s)",
          TokName(n->op.op),
          NodeReprShort(n->op.left->type),
          NodeReprShort(n->op.right->type));
        break;
      }
      n->type = n->op.left->type;
    }
    break;

  default:
    dlog("[convlit] ignoring n->kind %s", NodeKindName(n->kind));
    break;
  }

  return NULL;
}


// // binOpOkforType returns true if binary op Tok is available for operands of type TypeCode.
// // Tok => TypeCode => bool
// static bool binOpOkforType[T_PRIM_OPS_END - T_PRIM_OPS_START - 1][TypeCode_INTRINSIC_NUM_END] = {
//   //                 bool    int8  uint8  int16 uint16 int32 uint32 int64 uint64 float32 float64
//   /* TPlus       */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TMinus      */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TStar       */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TSlash      */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TGt         */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TLt         */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TEqEq       */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TNEq        */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TLEq        */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TGEq        */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TPlusPlus   */{ 0 },
//   /* TMinusMinus */{ 0 },
//   /* TTilde      */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
//   /* TNot        */{ false,  true, true,  true, true,  true, true,  true, true,  true,   true },
// };

// okfor[OADD] = okforadd[:]
// okfor[OAND] = okforand[:]
// okfor[OANDAND] = okforbool[:]
// okfor[OANDNOT] = okforand[:]
// okfor[ODIV] = okforarith[:]
// okfor[OEQ] = okforeq[:]
// okfor[OGE] = okforcmp[:]
// okfor[OGT] = okforcmp[:]
// okfor[OLE] = okforcmp[:]
// okfor[OLT] = okforcmp[:]
// okfor[OMOD] = okforand[:]
// okfor[OMUL] = okforarith[:]
// okfor[ONE] = okforeq[:]
// okfor[OOR] = okforand[:]
// okfor[OOROR] = okforbool[:]
// okfor[OSUB] = okforarith[:]
// okfor[OXOR] = okforand[:]
// okfor[OLSH] = okforand[:]
// okfor[ORSH] = okforand[:]

// func operandType(op Op, t *types.Type) *types.Type {
//   if okfor[op][t.Etype] {
//     return t
//   }
//   return nil
// }

