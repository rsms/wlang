#pragma once

typedef enum IROp {
  OpInvalid,
  OpUnknown,
  OpPhi,
  OpCopy,

  OpConstBool,
  OpConstI8,
  OpConstI16,
  OpConstI32,
  OpConstI64,
  OpConstF32,
  OpConstF64,

  OpCall,
} IROp;
