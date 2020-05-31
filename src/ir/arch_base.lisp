; Describes generic IR
;
(name     Generic)
(addrSize 4)
(regSize  4)
(intSize  4)
;
; Available operator attributes:
;   ZeroWidth           dummy op; no actual I/O.
;   Constant            true if the value is a constant. Value in aux
;   Commutative         commutative on its first 2 arguments (e.g. addition; x+y==y+x)
;   ResultInArg0        output of v and v.args[0] must be allocated to the same register.
;   ResultNotInArgs     outputs must not be allocated to the same registers as inputs
;   Rematerializeable   register allocator can recompute value instead of spilling/restoring.
;   ClobberFlags        this op clobbers flags register
;   Call                is a function call
;   NilCheck            this op is a nil check on arg0
;   FaultOnNilArg0      this op will fault if arg0 is nil (and aux encodes a small offset)
;   FaultOnNilArg1      this op will fault if arg1 is nil (and aux encodes a small offset)
;   UsesScratch         this op requires scratch memory space
;   HasSideEffects      for "reasons", not to be eliminated.  E.g., atomic store.
;   Generic             generic op
;
; Types:
;   bool     bit or byte sized integer
;   i8       sign-agnostic 8-bit integer
;   s8       signed 8-bit integer
;   u8       unsigned 8-bit integer
;   i16      sign-agnostic 16-bit integer
;   s16      signed 16-bit integer
;   u16      unsigned 16-bit integer
;   i32      sign-agnostic 32-bit integer
;   s32      signed 32-bit integer
;   u32      unsigned 32-bit integer
;   i64      sign-agnostic 64-bit integer
;   s64      signed 64-bit integer
;   u64      unsigned 64-bit integer
;   addr     address (address-sized integer)
;   f32      32-bit floating point number
;   f64      64-bit floating point number
;   mem      memory location
;   nil      void
;
(ops
  (Nil ()->() ZeroWidth)
  (Phi ()->() ZeroWidth) ; select an argument based on which predecessor block we came from
  ;
  ; Constant values. Stored in IRValue.aux
  (ConstBool  () -> bool  Constant  (aux bool))  ; aux is 0=false, 1=true
  (ConstI8    () -> i8    Constant  (aux i8))  ; aux is sign-extended 8 bits
  (ConstI16   () -> i16   Constant  (aux i16)) ; aux is sign-extended 16 bits
  (ConstI32   () -> i32   Constant  (aux i32)) ; aux is sign-extended 32 bits
  (ConstI64   () -> i64   Constant  (aux i64)) ; aux is Int64
  (ConstF32   () -> f32   Constant  (aux f32))
  (ConstF64   () -> f64   Constant  (aux f64))
  ;
  ; ---------------------------------------------------------------------
  ; 2-input arithmetic. Types must be consistent.
  ; Add for example must take two values of the same type and produce that same type.
  ;
  ; arg0 + arg1 ; sign-agnostic addition
  (AddI8   (i8  i8)  -> i8   Commutative  ResultInArg0)
  (AddI16  (i16 i16) -> i16  Commutative  ResultInArg0)
  (AddI32  (i32 i32) -> i32  Commutative  ResultInArg0)
  (AddI64  (i64 i64) -> i64  Commutative  ResultInArg0)
  (AddF32  (f32 f32) -> f32  Commutative  ResultInArg0)
  (AddF64  (f64 f64) -> f64  Commutative  ResultInArg0)
  ;
  ; arg0 - arg1 ; sign-agnostic subtraction
  (SubI8   (i8  i8)  -> i8   ResultInArg0)
  (SubI16  (i16 i16) -> i16  ResultInArg0)
  (SubI32  (i32 i32) -> i32  ResultInArg0)
  (SubI64  (i64 i64) -> i64  ResultInArg0)
  (SubF32  (f32 f32) -> f32  ResultInArg0)
  (SubF64  (f64 f64) -> f64  ResultInArg0)
  ;
  ; arg0 * arg1 ; sign-agnostic multiplication
  (MulI8   (i8  i8)  -> i8   Commutative  ResultInArg0)
  (MulI16  (i16 i16) -> i16  Commutative  ResultInArg0)
  (MulI32  (i32 i32) -> i32  Commutative  ResultInArg0)
  (MulI64  (i64 i64) -> i64  Commutative  ResultInArg0)
  (MulF32  (f32 f32) -> f32  Commutative  ResultInArg0)
  (MulF64  (f64 f64) -> f64  Commutative  ResultInArg0)
  ;
  ; arg0 / arg1 ; division
  (DivS8   (s8  s8)  -> s8   ResultInArg0) ; signed (result is truncated toward zero)
  (DivU8   (u8  u8)  -> u8   ResultInArg0) ; unsigned (result is floored)
  (DivS16  (s16 s16) -> s16  ResultInArg0)
  (DivU16  (u16 u16) -> u16  ResultInArg0)
  (DivS32  (s32 s32) -> s32  ResultInArg0)
  (DivU32  (u32 u32) -> u32  ResultInArg0)
  (DivS64  (s64 s64) -> s64  ResultInArg0)
  (DivU64  (u64 u64) -> u64  ResultInArg0)
  (DivF32  (f32 f32) -> f32  ResultInArg0)
  (DivF64  (f64 f64) -> f64  ResultInArg0)
  ;
  ; ---------------------------------------------------------------------
  ; 2-input comparisons
  ;
  ; arg0 == arg1 ; sign-agnostic compare equal
  (EqB    (bool bool) -> bool  Commutative)
  (EqI8   (i8 i8)   -> bool    Commutative)
  (EqI16  (i16 i16) -> bool    Commutative)
  (EqI32  (i32 i32) -> bool    Commutative)
  (EqI64  (i64 i64) -> bool    Commutative)
  (EqF32  (f32 f32) -> bool    Commutative)
  (EqF64  (f64 f64) -> bool    Commutative)
  ;
  ; arg0 != arg1 ; sign-agnostic compare unequal
  (NEqB    (bool bool) -> bool Commutative)
  (NEqI8   (i8 i8)   -> bool   Commutative)
  (NEqI16  (i16 i16) -> bool   Commutative)
  (NEqI32  (i32 i32) -> bool   Commutative)
  (NEqI64  (i64 i64) -> bool   Commutative)
  (NEqF32  (f32 f32) -> bool   Commutative)
  (NEqF64  (f64 f64) -> bool   Commutative)
  ;
  ; arg0 < arg1 ; less than
  (LessS8    (s8 s8) -> bool) ; signed
  (LessU8    (u8 u8) -> bool) ; unsigned
  (LessS16   (s16 s16) -> bool)
  (LessU16   (u16 u16) -> bool)
  (LessS32   (s32 s32) -> bool)
  (LessU32   (u32 u32) -> bool)
  (LessS64   (s64 s64) -> bool)
  (LessU64   (u64 u64) -> bool)
  (LessF32   (f32 f32) -> bool)
  (LessF64   (f64 f64) -> bool)
  ;
  ; arg0 > arg1 ; greater than
  (GreaterS8   (s8 s8) -> bool) ; signed
  (GreaterU8   (u8 u8) -> bool) ; unsigned
  (GreaterS16  (s16 s16) -> bool)
  (GreaterU16  (u16 u16) -> bool)
  (GreaterS32  (s32 s32) -> bool)
  (GreaterU32  (u32 u32) -> bool)
  (GreaterS64  (s64 s64) -> bool)
  (GreaterU64  (u64 u64) -> bool)
  (GreaterF32  (f32 f32) -> bool)
  (GreaterF64  (f64 f64) -> bool)
  ;
  ; arg0 <= arg1 ; less than or equal
  (LEqS8   (s8 s8) -> bool) ; signed
  (LEqU8   (u8 u8) -> bool) ; unsigned
  (LEqS16  (s16 s16) -> bool)
  (LEqU16  (u16 u16) -> bool)
  (LEqS32  (s32 s32) -> bool)
  (LEqU32  (u32 u32) -> bool)
  (LEqS64  (s64 s64) -> bool)
  (LEqU64  (u64 u64) -> bool)
  (LEqF32  (f32 f32) -> bool)
  (LEqF64  (f64 f64) -> bool)
  ;
  ; arg0 <= arg1 ; greater than or equal
  (GEqS8   (s8 s8) -> bool) ; signed
  (GEqU8   (u8 u8) -> bool) ; unsigned
  (GEqS16  (s16 s16) -> bool)
  (GEqU16  (u16 u16) -> bool)
  (GEqS32  (s32 s32) -> bool)
  (GEqU32  (u32 u32) -> bool)
  (GEqS64  (s64 s64) -> bool)
  (GEqU64  (u64 u64) -> bool)
  (GEqF32  (f32 f32) -> bool)
  (GEqF64  (f64 f64) -> bool)
  ;
  ; arg1 && arg2 ; both are true
  (AndB  (bool bool) -> bool  Commutative)
  ;
  ; arg1 || arg2 ; one of them are true
  (OrB  (bool bool) -> bool   Commutative)
  ;
  ; ---------------------------------------------------------------------
  ; 1-input
  ;
  ; !arg0 ; boolean not
  (NotB  bool -> bool)
  ;
  ; -arg0 ; negation
  (NegI8  i8 -> i8)
  (NegI16 i16 -> i16)
  (NegI32 i32 -> i32)
  (NegI64 i64 -> i64)
  (NegF32 f32 -> f32)
  (NegF64 f64 -> f64)
) ; ops
