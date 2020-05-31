#include "../wp.h"
#include "builder.h"


static bool addTopLevel(IRBuilder* u, Node* n);


void IRBuilderInit(IRBuilder* u, IRBuilderFlags flags, const char* pkgname) {
  memset(u, 0, sizeof(IRBuilder));
  u->mem = MemoryNew(0);
  u->pkg = IRPkgNew(u->mem, pkgname);
  PtrMapInit(&u->funs, 32, u->mem);
  u->vars = SymMapNew(8, u->mem);
  u->flags = flags;
  ArrayInitWithStorage(&u->defvars, u->defvarsStorage, sizeof(u->defvarsStorage)/sizeof(void*));
}

void IRBuilderFree(IRBuilder* u) {
  MemoryFree(u->mem);
}

bool IRBuilderAdd(IRBuilder* u, const CCtx* cc, Node* n) {
  u->cc = cc;
  bool ok = addTopLevel(u, n);
  u->cc = NULL;
  return ok;
}


static void errorf(IRBuilder* u, SrcPos pos, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  auto msg = sdsempty();
  if (strlen(format) > 0) {
    msg = sdscatvprintf(msg, format, ap);
    assert(sdslen(msg) > 0); // format may contain %S which is not supported by sdscatvprintf
  }
  va_end(ap);
  if (u->cc) {
    u->cc->errh(&u->cc->src, pos, msg, u->cc->userdata);
  }
  sdsfree(msg);
}



// sealBlock sets b.sealed=true, indicating that no further predecessors will be added
// (no changes to b.preds)
static void sealBlock(IRBuilder* u, IRBlock* b) {
  assert(!b->sealed); // block not sealed already
  dlog("sealBlock %p", b);
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
  b->sealed = true;
}

// startBlock sets the current block we're generating code in
static void startBlock(IRBuilder* u, IRBlock* b) {
  assert(u->b == NULL); // err: starting block without ending block
  u->b = b;
  dlog("startBlock %p", b);
}

// startSealedBlock is a convenience for sealBlock followed by startBlock
static void startSealedBlock(IRBuilder* u, IRBlock* b) {
  sealBlock(u, b);
  startBlock(u, b);
}

// endBlock marks the end of generating code for the current block.
// Returns the (former) current block. Returns null if there is no current
// block, i.e. if no code flows to the current execution point.
// The block sealed if not already sealed.
static IRBlock* endBlock(IRBuilder* u) {
  auto b = u->b;
  assert(b != NULL); // has current block

  // Move block-local vars to long-term definition data.
  // First we fill any holes in defvars.
  while (u->defvars.len <= b->id) {
    ArrayPush(&u->defvars, NULL, u->mem);
  }
  if (u->vars->len > 0) {
    u->defvars.v[b->id] = u->vars;
    u->vars = SymMapNew(8, u->mem);  // new block-local vars
  }

  #if DEBUG
  u->b = NULL;  // crash if we try to use b before a new block is started
  #endif

  return b;
}


static void startFun(IRBuilder* u, IRFun* f) {
  assert(u->f == NULL); // starting function with existing function
  u->f = f;
  dlog("startFun %p", u->f);
}

static void endFun(IRBuilder* u) {
  assert(u->f != NULL); // no current function
  dlog("endFun %p", u->f);
  #if DEBUG
  u->f = NULL;
  #endif
}


static IRValue* TODO_Value(IRBuilder* u) {
  return IRValueNew(u->f, u->b, OpNil, TypeCode_nil, /*SrcPos*/NULL);
}


// IntrinsicTypeCode takes any numberic TypeCode and returns a concrete type code like int32.
// Note: Currently int and uint are hard-coded to 32-bit ints. If we want to make this
// configurable, make this function part of the builder (take a builder arg) and return
// values set in the builder struct.
static TypeCode IntrinsicTypeCode(TypeCode t) {
  assert(t < TypeCode_NUM_END);
  switch (t) {
    case TypeCode_int:  return TypeCode_int32;
    case TypeCode_uint: return TypeCode_uint32;
    default:            return t;
  }
}


// ———————————————————————————————————————————————————————————————————————————————————————————————
// Phi & variables

#define dlogvar(format, ...) \
  fprintf(stderr, "VAR " format "\t(%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)


static void writeVariable(IRBuilder* u, Sym name, IRValue* value, IRBlock* b) {
  if (b == u->b) {
    dlogvar("write %.*s in current block", (int)symlen(name), name);
    auto oldv = SymMapSet(u->vars, name, value);
    if (oldv != NULL) {
      dlogvar("new value replaced old value: %p", oldv);
    }
  } else {
    dlogvar("TODO write %.*s in defvars", (int)symlen(name), name);
  }
}


static IRValue* readVariable(IRBuilder* u, Sym name, Node* typeNode, IRBlock* b/*null*/) {
  if (b == u->b) {
    // current block
    dlogvar("read %.*s in current block", (int)symlen(name), name);
    auto v = SymMapGet(u->vars, name);
    if (v != NULL) {
      return v;
    }
  } else {
    dlogvar("TODO read %.*s in defvars", (int)symlen(name), name);
  //   let m = u.defvars[b.id]
  //   if (m) {
  //     let v = m.get(name)
  //     if (v) {
  //       return v
  //     }
  //   }
  }

  dlogvar("read %.*s not found -- falling back to readRecursive", (int)symlen(name), name);

  // // global value numbering
  // return u.readVariableRecursive(name, t, b)

  return TODO_Value(u);
}


// ———————————————————————————————————————————————————————————————————————————————————————————————
// add functions. Most atomic at top, least at bottom. I.e. let -> block -> fun -> file.

static IRValue* addExpr(IRBuilder* u, Node* n);


static IRValue* addInt(IRBuilder* u, Node* n) {
  assert(n->kind == NInt);
  assert(n->type->kind == NBasicType);
  auto tc = IntrinsicTypeCode(n->type->t.basic.typeCode);
  return IRFunGetConstInt(u->f, tc, n->integer);
}


static IRValue* addAssign(IRBuilder* u, Sym name /*nullable*/, IRValue* value) {
  assert(value != NULL);
  if (name == NULL) {
    // dummy assignment to "_"; i.e. "_ = x" => "x"
    return value;
  }

  // instead of issuing an intermediate "copy", simply associate variable
  // name with the value on the right-hand side.
  writeVariable(u, name, value, u->b);

  if (u->flags & IRBuilderComments) {
    IRValueAddComment(value, u->mem, name);
  }

  return value;
}


static IRValue* addIdent(IRBuilder* u, Node* n) {
  assert(n->kind == NIdent);
  return readVariable(u, n->ref.name, n->type, u->b);
}


static IRValue* addOp(IRBuilder* u, Node* n) {
  assert(n->kind == NOp);

  dlog("addOp %s %s = %s",
    TokName(n->op.op),
    NodeReprShort(n->op.left),
    n->op.right != NULL ? NodeReprShort(n->op.right) : "nil"
  );

  // gen left operand
  auto left = addExpr(u, n->op.left);

  // gen right operand
  // RHS is NULL for PostfixOp. Note that PrefixOp has a dedicated AST type NPrefixOp.
  TypeCode type2 = TypeCode_nil;
  IRValue* right = NULL;
  if (n->op.right != NULL) {
    right = addExpr(u, n->op.right);
    type2 = right->type;
  }

  // lookup IROp
  IROp op = IROpFromAST(n->op.op, left->type, type2);
  assert(op != OpNil);

  // read result type
  #if DEBUG
  // ensure that the type we think n will have is actually the type of the resulting value.
  TypeCode restype = IROpInfo(op)->outputType;
  if (restype > TypeCode_INTRINSIC_NUM_END) {
    // result type is parametric; is the same as an input type.
    assert(restype == TypeCode_param1 || restype == TypeCode_param2);
    if (restype == TypeCode_param1) {
      restype = left->type;
    } else {
      restype = type2;
    }
  }
  assert(IntrinsicTypeCode(n->type->t.basic.typeCode) == restype);
  #else
  TypeCode restype = IntrinsicTypeCode(n->type->t.basic.typeCode);
  #endif

  return IRValueNew(u->f, u->b, op, restype, /*SrcPos*/NULL);
}


static IRValue* addLet(IRBuilder* u, Node* n) {
  assert(n->kind == NLet);
  assert(n->field.init != NULL);
  dlog("addLet %s %s = %s",
    n->field.name ? n->field.name : "_",
    NodeReprShort(n->type),
    n->field.init ? NodeReprShort(n->field.init) : "nil"
  );
  auto v = addExpr(u, n->field.init); // right-hand side
  return addAssign(u, n->field.name, v);
}


static IRValue* addBlock(IRBuilder* u, Node* n) {  // language block, not IR block.
  assert(n->kind == NBlock);
  IRValue* v = NULL;
  NodeListForEach(&n->array.a, n, {
    v = addExpr(u, n);
  });
  return v;
}


static IRValue* addExpr(IRBuilder* u, Node* n) {
  assert(n->type != NULL); // AST should be fully typed
  switch (n->kind) {
    case NBlock: return addBlock(u, n);
    case NLet:   return addLet(u, n);
    case NInt:   return addInt(u, n);
    case NOp:    return addOp(u, n);
    case NIdent: return addIdent(u, n);

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
    case NIf:
    case NNil:
    case NPrefixOp:
    case NReturn:
    case NTuple:
    case NTupleType:
    case NVar:
    case NZeroInit:
    case _NodeKindMax:
      dlog("TODO addExpr kind %s", NodeKindName(n->kind));
      return TODO_Value(u);
  }
  return NULL;
}


static IRFun* addFun(IRBuilder* u, Node* n) {
  assert(n->kind == NFun);
  assert(n->fun.body != NULL); // not a concrete function

  auto f = (IRFun*)PtrMapGet(&u->funs, (void*)n);
  if (f != NULL) {
    // fun already built or in progress of being built
    return f;
  }

  dlog("addFun %s", NodeReprShort(n));

  // allocate a new function and its entry block
  f = IRFunNew(u->mem, n);
  auto entryb = IRBlockNew(f, IRBlockPlain, &n->pos);

  // Since functions can be anonymous and self-referential, short-circuit using a PtrMap
  PtrMapSet(&u->funs, n, f);
  IRPkgAddFun(u->pkg, f);

  // start function
  startFun(u, f);
  startSealedBlock(u, entryb); // entry block has no predecessors, so seal right away.

  // build body
  auto bodyval = addExpr(u, n->fun.body);

  // end last block (if not already ended)
  if (u->b != NULL) {
    u->b->kind = IRBlockRet;
    if (n->fun.body->kind != NBlock) {
      // body is a single expression -- control value is that expression
      IRBlockSetControl(u->b, bodyval);
    }
    // when body is a block and it didn't end, it was empty and thus
    // the return type is nil (no control value.)
    endBlock(u);
  }

  // end function and return
  endFun(u);
  return f;
}


static bool addFile(IRBuilder* u, Node* n) {
  assert(n->kind == NFile);
  dlog("addFile %s", u->cc->src.name);
  NodeListForEach(&n->array.a, n, {
    if (!addTopLevel(u, n)) {
      return false;
    }
  });
  return true;
}


static bool addTopLevel(IRBuilder* u, Node* n) {
  switch (n->kind) {
    case NFile: return addFile(u, n);
    case NFun:  return addFun(u, n) != NULL;

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
      errorf(u, n->pos, "invalid top-level AST node %s", NodeKindName(n->kind));
      break;
  }
  return false;
}

