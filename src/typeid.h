#pragma once
#include "common/defs.h"
#include "types.h"
#include "sym.h"

typedef struct Node Node;

// GetTypeID retrieves the TypeID for the type node n.
// This function may mutate n by computing and storing id to n.t.id.
Sym GetTypeID(Node* n);

// TypeEquals returns true if a and b are equivalent types (i.e. identical).
bool TypeEquals(Node* a, Node* b);

// TypeConv describes the effect of converting one type to another
typedef enum TypeConv {
  TypeConvLossless = 0,  // conversion is "perfect". e.g. int32 -> int64
  TypeConvLossy,         // conversion may be lossy. e.g. int32 -> float32
  TypeConvImpossible,    // conversion is not possible. e.g. (int,int) -> bool
} TypeConv;

// // TypeConversion returns the effect of converting fromType -> toType.
// // intsize is the size in bytes of the "int" and "uint" types. E.g. 4 for 32-bit.
// TypeConv CheckTypeConversion(Node* fromType, Node* toType, u32 intsize);
