#pragma once
#include "../fwalloc.h"
#include "op.h"


typedef enum IRBlockKind {
  IRBlockInvalid = 0,
  IRBlockPlain,    // a single successor
  IRBlockIf,       // 2 successors, if control goto succs[0] else goto succs[1]
  IRBlockRet,      // no successors, control value is memory result
  IRBlockFirst,    // 2 successors, always takes the first one (second is dead)
} IRBlockKind;

typedef enum IRBranchPrediction {
  IRBranchUnlikely = -1,
  IRBranchUnknown  = 0,
  IRBranchLikely   = 1,
} IRBranchPrediction;


typedef struct IRFun IRFun;


// Edge represents a CFG edge
typedef struct IREdge { int TODO; } IREdge;


// IRConstCache is used internally by IRFun (fun.c) and holds constants
typedef struct IRConstCache {
  u32   bmap;       // maps TypeCode => branch array index
  void* branches[]; // dense branch array
} IRConstCache;


typedef struct IRValue {
  u32      id;   // unique identifier
  IROp     op;   // operation that computes this value
  TypeCode type;
  SrcPos   pos;  // source position

  u32 uses; // use count. Each appearance in args or IRBlock.control counts once.
  const char* comment; // short comment for IR formatting. Likely NULL. (FWAlloc)
} IRValue;


// Block represents a basic block
typedef struct IRBlock {
  IRFun*      f;       // owning function
  u32         id;      // block ID
  IRBlockKind kind;    // kind of block
  bool        sealed;  // true if no further predecessors will be added
  SrcPos      pos;     // source position
  Str         comment; // short comment for IR formatting. May be NULL.

  // three-address code values
  Array values; void* valuesStorage[8]; // IRValue*[]

  // control is a value that determines how the block is exited.
  // Its value depends on the kind of the block. For instance, a IRBlockIf has a boolean
  // control value and IRBlockExit has a memory control value.
  IRValue* control;

} IRBlock;


// Fun represents a function
typedef struct IRFun {
  FWAllocator* mem; // owning allocator
  IRBlock* entry;
  Array    blocks; void* blocksStorage[8]; // IRBlock*[]
  Sym      name;   // may be NULL
  SrcPos   pos;    // source position
  u32      nargs;  // number of arguments

  // internal; valid only during building
  u32    bid;    // block ID allocator
  u32    vid;    // value ID allocator
  Node*  type;   // .kind=NFunType
  IRConstCache* consts; // constants cache maps type+value => IRValue
} IRFun;


// Pkg represents a package with functions and data
typedef struct IRPkg {
  int TODO;
} IRPkg;


IRValue* IRValueNew(IRFun* f, IROp op, TypeCode type, const SrcPos*/*nullable*/ pos);
void     IRValueAddComment(IRValue* v, FWAllocator* a, ConstStr comment);

IRBlock* IRBlockNew(IRFun* f, IRBlockKind, const SrcPos*/*nullable*/);
void     IRBlockAddValue(IRBlock* b, IRValue* v);

IRFun*   IRFunNew(FWAllocator* a, Node* n);
IRValue* IRFunGetConstInt(IRFun* f, TypeCode t, u64 n);
IRValue* IRFunGetConstFloat(IRFun* f, TypeCode t, double n);


// Note: Must use the same FWAllocator for all calls to the same IRConstCache.
// Note: addHint is only valid until the next call to a mutating function like IRConstCacheAdd.
IRValue* IRConstCacheGet(
  const IRConstCache* c, FWAllocator* a, TypeCode t, intptr_t value, int* out_addHint);
IRConstCache* IRConstCacheAdd(
  IRConstCache* c, FWAllocator* a, TypeCode t, intptr_t value, IRValue* v, int addHint);
