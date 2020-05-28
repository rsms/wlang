#include "../wp.h"
#include "builder.h"

static bool addTopLevel(IRBuilder* b, Node* n);

// inline static PtrMap* PtrMapNew(size_t initbuckets) {
//   auto m = (PtrMap*)malloc(sizeof(PtrMap));
//   PtrMapInit(m, initbuckets);
//   return m;
// }

inline static PtrMap* PtrMapNew(FWAllocator* a, size_t initbuckets) {
  auto m = (PtrMap*)FWAlloc(a, sizeof(PtrMap));
  PtrMapInit(m, initbuckets);
  return m;
}


void IRBuilderInit(IRBuilder* b, IRBuilderFlags flags) {
  memset(b, 0, sizeof(IRBuilder));
  FWAllocInit(&b->mem);
  PtrMapInit(&b->funs, 32);
  b->vars = PtrMapNew(&b->mem, 32);
  b->flags = flags;
  ArrayInitWithStorage(&b->defvars, b->defvarsStorage, sizeof(b->defvarsStorage)/sizeof(void*));
}

bool IRBuilderAdd(IRBuilder* b, const CCtx* cc, Node* n) {
  b->cc = cc;
  bool ok = addTopLevel(b, n);
  b->cc = NULL;
  return ok;
}


static void errorf(IRBuilder* b, SrcPos pos, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  auto msg = sdsempty();
  if (strlen(format) > 0) {
    msg = sdscatvprintf(msg, format, ap);
    assert(sdslen(msg) > 0); // format may contain %S which is not supported by sdscatvprintf
  }
  va_end(ap);
  if (b->cc) {
    b->cc->errh(&b->cc->src, pos, msg, b->cc->userdata);
  }
  sdsfree(msg);
}



// sealBlock sets b.sealed=true, indicating that no further predecessors will be added
// (no changes to b.preds)
static void sealBlock(IRBuilder* b, IRBlock* block) {
  assert(!block->sealed); // block not sealed already
  dlog("sealBlock %p", block);
  // TODO: port from co:
  // if (b->incompletePhis != NULL) {
  //   let entries = s.incompletePhis.get(b)
  //   if (entries) {
  //     for (let [name, phi] of entries) {
  //       dlogPhi(`complete pending phi ${phi} (${name})`)
  //       s.addPhiOperands(name, phi)
  //     }
  //     s.incompletePhis.delete(b)
  //   }
  // }
  block->sealed = true;
}

// startBlock sets the current block we're generating code in
static void startBlock(IRBuilder* b, IRBlock* block) {
  assert(b->b == NULL); // err: starting block without ending block
  b->b = block;
  dlog("startBlock %p", block);
}

// startSealedBlock is a convenience for sealBlock followed by startBlock
static void startSealedBlock(IRBuilder* b, IRBlock* block) {
  sealBlock(b, block);
  startBlock(b, block);
}

// endBlock marks the end of generating code for the current block.
// Returns the (former) current block. Returns null if there is no current
// block, i.e. if no code flows to the current execution point.
// The block sealed if not already sealed.
static IRBlock* endBlock(IRBuilder* b) {
  auto block = b->b;
  assert(block != NULL); // has current block

  // Move block-local vars to long-term definition data.
  // First we fill any holes in defvars.
  while (b->defvars.len <= block->id) {
    ArrayPush(&b->defvars, NULL, &b->mem);
  }
  if (b->vars->len > 0) {
    b->defvars.v[block->id] = b->vars;
    b->vars = PtrMapNew(&b->mem, 32);  // new block-local vars
  }

  #if DEBUG
  b->b = NULL;  // crash if we try to use b before a new block is started
  #endif

  return block;
}


static void startFun(IRBuilder* b, IRFun* f) {
  assert(b->f == NULL); // starting function with existing function
  b->f = f;
  dlog("startFun %p", b->f);
}

static void endFun(IRBuilder* b) {
  assert(b->f != NULL); // no current function
  dlog("endFun %p", b->f);
  #if DEBUG
  b->f = NULL;
  #endif
}


static void IRBlockSetControl(IRBlock* b, IRValue* v) {
  if (b->control) {
    b->control->uses--;
  }
  b->control = v;
  if (v) {
    v->uses++;
  }
}


static void writeVariable(IRBuilder* b, Sym name, IRValue* value) {
  dlog("TODO writeVariable %.*s", (int)symlen(name), name);
}


// ———————————————————————————————————————————————————————————————————————————————————————————————
// add functions. Most atomic at top, least at bottom. I.e. let -> block -> fun -> file.

static IRValue* addExpr(IRBuilder* b, Node* n);


static IRValue* addInt(IRBuilder* b, Node* n) {
  assert(n->kind == NInt);
  assert(n->type != NULL);
  assert(n->type->kind == NBasicType);
  return IRFunGetConstInt(b->f, n->type->t.basic.typeCode, n->integer);
}


static IRValue* addAssign(IRBuilder* b, Sym name /*nullable*/, IRValue* value) {
  assert(value != NULL);
  if (name == NULL) {
    // dummy assignment to "_"; i.e. "_ = x" => "x"
    return value;
  }

  // instead of issuing an intermediate "copy", simply associate variable
  // name with the value on the right-hand side.
  writeVariable(b, name, value);

  if (b->flags & IRBuilderComments) {
    IRValueAddComment(value, &b->mem, name);
  }

  return NULL;
}


static IRValue* addLet(IRBuilder* b, Node* n) {
  assert(n->kind == NLet);
  assert(n->field.init != NULL);

  dlog("addLet %s %s = %s",
    n->field.name ? n->field.name : "_",
    NodeReprShort(n->type),
    n->field.init ? NodeReprShort(n->field.init) : "nil"
  );

  auto v = addExpr(b, n->field.init); // right-hand side
  v = addAssign(b, n->field.name, v);

  return v;
}


static IRValue* addBlock(IRBuilder* b, Node* n) {  // language block, not IR block.
  assert(n->kind == NBlock);
  IRValue* v = NULL;
  NodeListForEach(&n->array.a, n, {
    v = addExpr(b, n);
  });
  return v;
}


static IRValue* addExpr(IRBuilder* b, Node* n) {
  switch (n->kind) {
    case NBlock: return addBlock(b, n);
    case NLet:   return addLet(b, n);
    case NInt:   return addInt(b, n);

    case NNone:
    case NBad:
    case NAssign:
    case NBasicType:
    case NBool:
    case NCall:
    case NComment:
    case NConst:
    case NField:
    case NFile:
    case NFloat:
    case NFun:
    case NFunType:
    case NIdent:
    case NIf:
    case NNil:
    case NOp:
    case NPrefixOp:
    case NReturn:
    case NTuple:
    case NTupleType:
    case NVar:
    case NZeroInit:
    case _NodeKindMax:
      dlog("TODO addExpr kind %s", NodeKindName(n->kind));
  }
  return NULL;
}


static IRFun* addFun(IRBuilder* b, Node* n) {
  assert(n->kind == NFun);
  assert(n->fun.body != NULL); // not a concrete function

  auto f = (IRFun*)PtrMapGet(&b->funs, (void*)n);
  if (f != NULL) {
    // fun already built or in progress of being built
    return f;
  }

  dlog("addFun %s", NodeReprShort(n));

  // allocate a new function and its entry block
  f = IRFunNew(&b->mem, n);
  f->entry = IRBlockNew(f, IRBlockPlain, &n->pos);

  // Since functions can be anonymous and self-referential, short-circuit using a PtrMap
  PtrMapSet(&b->funs, n, f);

  // start function
  startFun(b, f);
  startSealedBlock(b, f->entry); // entry block has no predecessors, so seal right away.

  // build body
  auto bodyval = addExpr(b, n->fun.body);

  // end last block (if not already ended)
  if (b->b != NULL) {
    b->b->kind = IRBlockRet;
    if (n->fun.body->kind != NBlock) {
      // body is a single expression -- control value is that expression
      IRBlockSetControl(b->b, bodyval);
    }
    // when body is a block and it didn't end, it was empty and thus
    // the return type is nil (no control value.)
    endBlock(b);
  }

  // end function and return
  endFun(b);
  return f;
}


static bool addFile(IRBuilder* b, Node* n) {
  assert(n->kind == NFile);
  dlog("addFile %s", b->cc->src.name);
  NodeListForEach(&n->array.a, n, {
    if (!addTopLevel(b, n)) {
      return false;
    }
  });
  return true;
}


static bool addTopLevel(IRBuilder* b, Node* n) {
  switch (n->kind) {
    case NFile: return addFile(b, n);
    case NFun:  return addFun(b, n) != NULL;

    case NLet:
      // top-level let bindings which are not exported can be ignored.
      // All let bindings are resolved already, so they only concern IR if their data is exported.
      // Since exporting is not implemented, just ignore top-level let for now.
      return true;

    case NNone:
    case NBad:
    case NAssign:
    case NBasicType:
    case NBlock:
    case NBool:
    case NCall:
    case NComment:
    case NConst:
    case NField:
    case NFloat:
    case NFunType:
    case NIdent:
    case NIf:
    case NInt:
    case NNil:
    case NOp:
    case NPrefixOp:
    case NReturn:
    case NTuple:
    case NTupleType:
    case NVar:
    case NZeroInit:
    case _NodeKindMax:
      errorf(b, n->pos, "invalid top-level AST node %s", NodeKindName(n->kind));
      break;
  }
  return false;
}

