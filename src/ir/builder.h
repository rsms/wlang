#pragma once
#include "ir.h"
#include "../array.h"
#include "../ptrmap.h"


typedef enum IRBuilderFlags {
  IRBuilderDefault  = 0,
  IRBuilderComments = 1 << 1,  // include comments in some values, for formatting
} IRBuilderFlags;


typedef struct IRBuilder {
  FWAllocator    mem;
  PtrMap         funs; // Node* => IRFun* -- generated functions
  IRBuilderFlags flags;
  IRPkg*         pkg;

  // state used during building
  const CCtx* cc; // current source context (source-file specific)
  IRBlock* b;     // current block
  IRFun*   f;     // current function

  PtrMap* vars; // Sym => IRValue*
    // variable assignments in the current block (map from variable symbol to ssa value)
    // this PtrMap is moved into defvars when a block ends (internal call to endBlock.)

  void* defvarsStorage[512]; // initial storage for defvars
  Array defvars; // PtrMap*[]  (from vars)
    // all defined variables at the end of each block. Indexed by block id.
    // null indicates there are no variables in that block.

  // incompletePhis :Map<Block,Map<ByteStr,Value>>|null
    // tracks pending, incomplete phis that are completed by sealBlock for
    // blocks that are sealed after they have started. This happens when preds
    // are not known at the time a block starts, but is known and registered
    // before the block ends.

} IRBuilder;

// start a new IRPkg.
// b must be zeroed memory or a reused builder.
void IRBuilderInit(IRBuilder* b, IRBuilderFlags flags);

// add ast to top-level of the current IRPkg. Returns false if any errors occured.
bool IRBuilderAdd(IRBuilder* b, const CCtx* cc, Node* ast);
