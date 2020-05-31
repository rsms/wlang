#pragma once

// Op___
typedef enum IROp {
  // generated by misc/gen_ops.py from src/ir/arch_base.lisp
  OpNil,
  OpPhi,	// select an argument based on which predecessor block we came from
  //
  // Constant values. Stored in IRValue.aux
  OpConstBool,	// aux is 0=false, 1=true
  OpConstI8,	// aux is sign-extended 8 bits
  OpConstI16,	// aux is sign-extended 16 bits
  OpConstI32,	// aux is sign-extended 32 bits
  OpConstI64,	// aux is Int64
  OpConstF32,
  OpConstF64,
  //
  // ---------------------------------------------------------------------
  // 2-input arithmetic. Types must be consistent.
  // Add for example must take two values of the same type and produce that same type.
  //
  // arg0 + arg1 ; sign-agnostic addition
  OpAddI8,
  OpAddI16,
  OpAddI32,
  OpAddI64,
  OpAddF32,
  OpAddF64,
  //
  // arg0 - arg1 ; sign-agnostic subtraction
  OpSubI8,
  OpSubI16,
  OpSubI32,
  OpSubI64,
  OpSubF32,
  OpSubF64,
  //
  // arg0 * arg1 ; sign-agnostic multiplication
  OpMulI8,
  OpMulI16,
  OpMulI32,
  OpMulI64,
  OpMulF32,
  OpMulF64,
  //
  // arg0 / arg1 ; division
  OpDivS8,	// signed (result is truncated toward zero)
  OpDivU8,	// unsigned (result is floored)
  OpDivS16,
  OpDivU16,
  OpDivS32,
  OpDivU32,
  OpDivS64,
  OpDivU64,
  OpDivF32,
  OpDivF64,
  //
  // ---------------------------------------------------------------------
  // 2-input comparisons
  //
  // arg0 == arg1 ; sign-agnostic compare equal
  OpEqB,
  OpEqI8,
  OpEqI16,
  OpEqI32,
  OpEqI64,
  OpEqF32,
  OpEqF64,
  //
  // arg0 != arg1 ; sign-agnostic compare unequal
  OpNEqB,
  OpNEqI8,
  OpNEqI16,
  OpNEqI32,
  OpNEqI64,
  OpNEqF32,
  OpNEqF64,
  //
  // arg0 < arg1 ; less than
  OpLessS8,	// signed
  OpLessU8,	// unsigned
  OpLessS16,
  OpLessU16,
  OpLessS32,
  OpLessU32,
  OpLessS64,
  OpLessU64,
  OpLessF32,
  OpLessF64,
  //
  // arg0 > arg1 ; greater than
  OpGreaterS8,	// signed
  OpGreaterU8,	// unsigned
  OpGreaterS16,
  OpGreaterU16,
  OpGreaterS32,
  OpGreaterU32,
  OpGreaterS64,
  OpGreaterU64,
  OpGreaterF32,
  OpGreaterF64,
  //
  // arg0 <= arg1 ; less than or equal
  OpLEqS8,	// signed
  OpLEqU8,	// unsigned
  OpLEqS16,
  OpLEqU16,
  OpLEqS32,
  OpLEqU32,
  OpLEqS64,
  OpLEqU64,
  OpLEqF32,
  OpLEqF64,
  //
  // arg0 <= arg1 ; greater than or equal
  OpGEqS8,	// signed
  OpGEqU8,	// unsigned
  OpGEqS16,
  OpGEqU16,
  OpGEqS32,
  OpGEqU32,
  OpGEqS64,
  OpGEqU64,
  OpGEqF32,
  OpGEqF64,
  //
  // arg1 && arg2 ; both are true
  OpAndB,
  //
  // arg1 || arg2 ; one of them are true
  OpOrB,
  //
  // ---------------------------------------------------------------------
  // 1-input
  //
  // !arg0 ; boolean not
  OpNotB,
  //
  // -arg0 ; negation
  OpNegI8,
  OpNegI16,
  OpNegI32,
  OpNegI64,
  OpNegF32,
  OpNegF64,

  Op_MAX
} IROp;


typedef enum IROpFlag {
} IROpFlag;


// IROpDescr describes an IROp. Retrieve with IROpInfo(IROp).
typedef struct IROpDescr {
  TypeCode outputType; // invariant: < TypeCode_INTRINSIC_NUM_END
  IROpFlag flags;
} IROpDescr;


// maps IROp => name, e.g. "Phi"
const char* const IROpNames[Op_MAX];

// IROpConstFromAST(TypeCode) maps TypeCode => IROp for constants.
const IROp _IROpConstMap[TypeCode_INTRINSIC_NUM_END];
static inline IROp IROpConstFromAST(TypeCode t) {
  assert(t < TypeCode_INTRINSIC_NUM_END); // t must be a concrete, basic numeric type
  return _IROpConstMap[t];
}

// IROpFromAST performs a lookup of IROp based on one or two inputs (type1 & type2)
// along with an AST operator (astOp). For single-input, set type2 to TypeCode_nil.
// Returns OpNil if there is no corresponding IR operation.
IROp IROpFromAST(Tok astOp, TypeCode type1, TypeCode type2);


// IROpInfo accesses the description of an IROp
const IROpDescr _IROpInfoMap[Op_MAX];
static inline const IROpDescr* IROpInfo(IROp op) {
  assert(op < Op_MAX);
  return &_IROpInfoMap[op];
}

// static inline IROp IROpFromASTOp1(Tok op, TypeCode t) {
//   assert(op > T_OPS_START);  // op must be an operator token
//   assert(op < T_OPS_END);
//   assert(t < TypeCode_INTRINSIC_NUM_END); // t must be a concrete, basic numeric type
//   auto irOpTable = astToIROpTable[op - T_OPS_START - 1];
//   assert(irOpTable != NULL);
//   return irOpTable[t];
// }

// static inline IROp IROpFromASTOp2(Tok op, TypeCode t1, TypeCode t2) {
//   assert(op > T_OPS_START);  // op must be an operator token
//   assert(op < T_OPS_END);
//   assert(t1 < TypeCode_INTRINSIC_NUM_END); // t must be a concrete, basic numeric type
//   assert(t2 < TypeCode_INTRINSIC_NUM_END);
//   auto irOpTable = astToIROpTable[op - T_OPS_START - 1];
//   assert(irOpTable != NULL);
//   return irOpTable[t1 * t2];
// }
