#include "../wp.h"
#include "builder.h"


static sds sdscatval(sds s, const IRValue* v, int indent) {
  s = sdscatfmt(s, "v%u(op=%s type=%s args=[", v->id, IROpName(v->op), TypeCodeName(v->type));
  if (v->argslen > 0) {
    for (int i = 0; i < v->argslen; i++) {
      s = sdscatprintf(s, "\n  %*s", (indent * 2), "");
      s = sdscatval(s, v->args[i], indent + 1);
    }
    s = sdscatprintf(s, "\n%*s", (indent * 2), "");
  }
  s = sdscat(s, "])");
  return s;
}

static ConstStr fmtval(const IRValue* v) {
  auto s = sdscatval(sdsnewcap(32), v, 0);
  return memgcsds(s); // GC
}


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

  dlog("TODO");
  return TODO_Value(u);
}


// ———————————————————————————————————————————————————————————————————————————————————————————————
// add functions. Most atomic at top, least at bottom. I.e. let -> block -> fun -> file.

static IRValue* addExpr(IRBuilder* u, Node* n);


static IRValue* addIntConst(IRBuilder* u, Node* n) {
  assert(n->kind == NIntLit);
  assert(n->type->kind == NBasicType);
  auto v = IRFunGetConstInt(u->f, n->type->t.basic.typeCode, n->val.i);
  return v;
}


static IRValue* addBoolConst(IRBuilder* u, Node* n) {
  assert(n->kind == NBoolLit);
  assert(n->type->kind == NBasicType);
  assert(n->type->t.basic.typeCode == TypeCode_bool);
  return IRFunGetConstBool(u->f, (bool)n->val.i);
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
  assert(n->ref.target != NULL); // never unresolved
  // dlog("addIdent \"%s\" target = %s", n->ref.name, fmtnode(n->ref.target));
  if (n->ref.target->kind == NLet) {
    // variable
    return readVariable(u, n->ref.name, n->type, u->b);
  }
  // else: type or builtin etc
  return addExpr(u, (Node*)n->ref.target);
}


static IRValue* addTypeCast(IRBuilder* u, Node* n) {
  assert(n->kind == NTypeCast);
  assert(n->call.receiver != NULL);
  assert(n->call.args != NULL);

  // generate rvalue
  auto inval = addExpr(u, n->call.args);
  auto dstType = n->call.receiver;

  if (dstType->kind != NBasicType) {
    CCtxErrorf(u->cc, n->pos, "invalid type %s in type cast", fmtnode(dstType));
    return TODO_Value(u);
  }

  auto totype = dstType->t.basic.typeCode;
  // aliases. e.g. int, uint
  switch (totype) {
    case TypeCode_int:  totype = TypeCode_int32; break;
    case TypeCode_uint: totype = TypeCode_uint32; break;
    default: break;
  }
  if (totype == inval->type) {
    return inval;
  }
  IROp convop = IROpConvertType(inval->type, totype);
  if (convop == OpNil) {
    CCtxErrorf(u->cc, n->pos, "invalid type conversion %s to %s",
      TypeCodeName(inval->type), TypeCodeName(dstType->t.basic.typeCode));
    return TODO_Value(u);
  }
  auto v = IRValueNew(u->f, u->b, convop, totype, &n->pos);
  IRValueAddArg(v, inval);
  return v;
}


static IRValue* addArg(IRBuilder* u, Node* n) {
  assert(n->kind == NArg);
  if (n->type->kind != NBasicType) {
    // TODO add support for NTupleType et al
    CCtxErrorf(u->cc, n->pos, "invalid argument type %s", fmtnode(n->type));
    return TODO_Value(u);
  }
  auto v = IRValueNew(u->f, u->b, OpArg, n->type->t.basic.typeCode, &n->pos);
  v->auxInt = n->field.index;
  return v;
}


static IRValue* addBinOp(IRBuilder* u, Node* n) {
  assert(n->kind == NBinOp);

  dlog("addBinOp %s %s = %s",
    TokName(n->op.op),
    fmtnode(n->op.left),
    n->op.right != NULL ? fmtnode(n->op.right) : "nil"
  );

  // gen left operand
  auto left  = addExpr(u, n->op.left);
  auto right = addExpr(u, n->op.right);

  dlog("[BinOp] left:  %s", fmtval(left));
  dlog("[BinOp] right: %s", fmtval(right));

  // lookup IROp
  IROp op = IROpFromAST(n->op.op, left->type, right->type);
  assert(op != OpNil);

  // read result type
  #if DEBUG
  // ensure that the type we think n will have is actually the type of the resulting value.
  TypeCode restype = IROpInfo(op)->outputType;
  if (restype > TypeCode_NUM_END) {
    // result type is parametric; is the same as an input type.
    assert(restype == TypeCode_param1 || restype == TypeCode_param2);
    if (restype == TypeCode_param1) {
      restype = left->type;
    } else {
      restype = right->type;
    }
  }
  assert( restype == n->type->t.basic.typeCode);
  #else
  TypeCode restype = n->type->t.basic.typeCode;
  #endif

  auto v = IRValueNew(u->f, u->b, op, restype, &n->pos);
  IRValueAddArg(v, left);
  IRValueAddArg(v, right);
  return v;
}


static IRValue* addLet(IRBuilder* u, Node* n) {
  assert(n->kind == NLet);
  assert(n->field.init != NULL);
  if (n->type == NULL || n->type == Type_ideal) {
    // this means the let binding is unused; the type resolver never bothered
    // resolving it as nothing referenced it.
    dlog("[ir/builder] discard unused let %s", fmtnode(n));
    return NULL;
  }
  dlog("addLet %s %s = %s",
    n->field.name ? n->field.name : "_",
    fmtnode(n->type),
    n->field.init ? fmtnode(n->field.init) : "nil"
  );
  auto v = addExpr(u, n->field.init); // right-hand side
  return addAssign(u, n->field.name, v);
}


// addIf reads an "if" expression, e.g.
//  (If (Op > (Ident x) (Int 1)) ; condition
//      (Let x (Int 1))          ; then block
//      (Let x (Int 2)) )        ; else block
// Returns a new empty block that's the block after the if.
static IRValue* addIf(IRBuilder* u, Node* n) {
  assert(n->kind == NIf);
  //
  // if..end has the following semantics:
  //
  //   if cond b1 b2
  //   b1:
  //     <then-block>
  //   goto b2
  //   b2:
  //     <continuation-block>
  //
  // if..else..end has the following semantics:
  //
  //   if cond b1 b2
  //   b1:
  //     <then-block>
  //   goto b3
  //   b2:
  //     <else-block>
  //   goto b3
  //   b3:
  //     <continuation-block>
  //
  // TODO

  // generate control condition
  auto control = addExpr(u, n->cond.cond);
  if (control->type != TypeCode_bool) {
    // AST should not contain conds that are non-bool
    CCtxErrorf(u->cc, n->cond.cond->pos,
      "invalid non-bool type in condition %s", fmtnode(n->cond.cond));
  }

  assert(n->cond.thenb != NULL);

  // [optimization] Early optimization of constant boolean condition
  if ((u->flags & IRBuilderOpt) != 0 && (IROpInfo(control->op)->aux & IRAuxBool)) {
    dlog("[ir/builder if] short-circuit constant cond");
    if (control->auxInt != 0) {
      // then branch always taken
      return addExpr(u, n->cond.thenb);
    }
    // else branch always taken
    if (n->cond.elseb == NULL) {
      dlog("TODO ir/builder produce nil value");
      return IRValueNew(u->f, u->b, OpNil, TypeCode_nil, &n->pos);
    }
    return addExpr(u, n->cond.elseb);
  }

  // end predecessor block (leading up to and including "if")
  auto ifb = endBlock(u);
  ifb->kind = IRBlockIf;
  IRBlockSetControl(ifb, control);

  // create blocks for then and else branches
  auto thenb = IRBlockNew(u->f, IRBlockCont, &n->cond.thenb->pos);
  auto elsebIndex = u->f->blocks.len; // may be used later for moving blocks
  auto elseb = IRBlockNew(u->f, IRBlockCont,
    n->cond.elseb == NULL ? &n->pos : &n->cond.elseb->pos);
  ifb->succs[0] = thenb;
  ifb->succs[1] = elseb; // if -> then, else

  // begin "then" block
  dlog("[if] begin \"then\" block");
  thenb->preds[0] = ifb; // then <- if
  startSealedBlock(u, thenb);
  auto thenv = addExpr(u, n->cond.thenb);  // generate "then" body
  thenb = endBlock(u);

  if (n->cond.elseb != NULL) {
    // "else"

    // allocate "cont" block; the block following both thenb and elseb
    auto contbIndex = u->f->blocks.len;
    auto contb = IRBlockNew(u->f, IRBlockCont, &n->pos);

    // begin "else" block
    dlog("[if] begin \"else\" block");
    elseb->preds[0] = ifb; // else <- if
    startSealedBlock(u, elseb);
    auto elsev = addExpr(u, n->cond.elseb);  // generate "else" body
    elseb = endBlock(u);
    elseb->succs[0] = contb; // else -> cont
    thenb->succs[0] = contb; // then -> cont
    contb->preds[0] = thenb;
    contb->preds[1] = elseb; // cont <- then, else
    startSealedBlock(u, contb);

    // move cont block to end (in case blocks were created by "else" body)
    IRFunMoveBlockToEnd(u->f, contbIndex);

    if (u->flags & IRBuilderComments) {
      thenb->comment = memsprintf(u->mem, "b%u.then", ifb->id);
      elseb->comment = memsprintf(u->mem, "b%u.else", ifb->id);
      contb->comment = memsprintf(u->mem, "b%u.end", ifb->id);
    }

    assertf(thenv->type == elsev->type,
      "branch type mismatch %s, %s", TypeCodeName(thenv->type), TypeCodeName(elsev->type));

    // make Phi
    auto phi = IRValueNew(u->f, u->b, OpPhi, thenv->type, &n->pos);
    assertf(u->b->preds[0] != NULL, "phi in block without predecessors");
    phi->args[0] = thenv;
    phi->args[1] = elsev;
    thenv->uses++;
    elsev->uses++;
    phi->argslen = 2;
    return phi;

  } else {
    // no "else" block
    thenb->succs[0] = elseb; // then -> else
    elseb->preds[0] = ifb;
    elseb->preds[1] = thenb; // else <- if, then
    startSealedBlock(u, elseb);

    // move ending block to end
    // r.f.blocks.copyWithin(elsebidx, elsebidx+1)
    // r.f.blocks[r.f.blocks.length-1] = elseb

    // move cont block to end (in case blocks were created by "then" body)
    IRFunMoveBlockToEnd(u->f, elsebIndex);

    if (u->flags & IRBuilderComments) {
      thenb->comment = memsprintf(u->mem, "b%u.then", ifb->id);
      elseb->comment = memsprintf(u->mem, "b%u.end", ifb->id);
    }

    // Consider and decide what semantics we want for if..then expressions without else.
    // There are at least three possibilities:
    //
    //   A. zero initialized value of the then-branch type:
    //
    //      "x = if y 3"                 typeof(x) => int       If false: 0
    //      "x = if y Account{ id: 1 }"  typeof(x) => Account   If false: Account{id:0}
    //
    //   B. zero initialized basic types, higher-level types become optional:
    //
    //      "x = if y 3"                 typeof(x) => int       If false: 0
    //      "x = if y Account{ id: 1 }"  typeof(x) => Account?  If false: nil
    //
    //   C. any type becomes optional:
    //
    //      "x = if y 3"                 typeof(x) => int?      If false: nil
    //      "x = if y Account{ id: 1 }"  typeof(x) => Account?  If false: nil
    //
    // Discussion:
    //
    //   C implies that the language has a concept of pointers beyond reference types.
    //   i.e. is an int? passed to a function copy-by-value or not? Probably not because then
    //   "fun foo(x int)" vs "fun foo(x int?)" would be equivalent, which doesn't make sense.
    //   Okay, so C is out.
    //
    //   B is likely the best choice here, assuming the language has a concept of optional.
    //   To implement B, we need to:
    //
    //   - Add support to resolve_type so that the effective type of the the if expression is
    //     optional for higher-level types (but not for basic types.)
    //
    //   - Decide on a representation of optional. Likely actually null; the constant 0.
    //     In that case, we have two options for IR branch block generation:
    //
    //       1. Store 0 to the result before evaluating the condition expression, or
    //
    //       2. generate an implicit "else" block that stores 0 to the result.
    //
    //     Approach 1 introduces possibly unnecessary stores while the second approach introduces
    //     Phi joints always. Also, the second approach introduces additional branching in the
    //     final generated code.
    //
    //     Because of this, approach 1 is the better one. It has optimization opportunities as
    //     well, like for instance if we know that the storage of the result of the if expression
    //     is already zero (e.g. from calloc), we can skip storing zero before the branch.
    //
    // Conclusion:
    //
    // - B. zero initialized basic types, higher-level types become optional.
    // - Store zero before branch, rather than generating implicit "else" branches.
    // - Introduce "optional" as a concept in the language.
    // - Update resolve_type to convert type to optional when there is no "else" branch.
    //
  }

  // TODO

  dlog("TODO");
  return TODO_Value(u);
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
  assert(n->kind == NLet || n->type != NULL); // AST should be fully typed (let is an exception)
  switch (n->kind) {
    case NLet:      return addLet(u, n);
    case NBlock:    return addBlock(u, n);
    case NIntLit:   return addIntConst(u, n);
    case NBoolLit:  return addBoolConst(u, n);
    case NBinOp:    return addBinOp(u, n);
    case NIdent:    return addIdent(u, n);
    case NIf:       return addIf(u, n);
    case NTypeCast: return addTypeCast(u, n);
    case NArg:      return addArg(u, n);

    case NFloatLit:
    case NNil:
    case NAssign:
    case NBasicType:
    case NCall:
    case NComment:
    case NField:
    case NFile:
    case NFun:
    case NFunType:
    case NPrefixOp:
    case NPostfixOp:
    case NReturn:
    case NTuple:
    case NTupleType:
    case NZeroInit:
      dlog("TODO addExpr kind %s", NodeKindName(n->kind));
      break;

    case NNone:
    case NBad:
    case _NodeKindMax:
      CCtxErrorf(u->cc, n->pos, "invalid AST node %s", NodeKindName(n->kind));
      break;
  }
  return TODO_Value(u);
}


static IRFun* addFun(IRBuilder* u, Node* n) {
  assert(n->kind == NFun);
  assert(n->fun.body != NULL); // not a concrete function

  auto f = (IRFun*)PtrMapGet(&u->funs, (void*)n);
  if (f != NULL) {
    // fun already built or in progress of being built
    return f;
  }

  dlog("addFun %s", fmtnode(n));

  // allocate a new function and its entry block
  f = IRFunNew(u->mem, n);
  auto entryb = IRBlockNew(f, IRBlockCont, &n->pos);

  // Since functions can be anonymous and self-referential, short-circuit using a PtrMap
  PtrMapSet(&u->funs, n, f);
  IRPkgAddFun(u->pkg, f);

  // start function
  startFun(u, f);
  startSealedBlock(u, entryb); // entry block has no predecessors, so seal right away.

  // build body
  auto bodyval = addExpr(u, n->fun.body);

  // end last block, if not already ended
  if (u->b != NULL) {
    u->b->kind = IRBlockRet;
    IRBlockSetControl(u->b, bodyval);
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
    case NBoolLit:
    case NIntLit:
    case NFloatLit:
    case NNil:
    case NAssign:
    case NBasicType:
    case NBlock:
    case NCall:
    case NComment:
    case NField:
    case NArg:
    case NFunType:
    case NIdent:
    case NIf:
    case NBinOp:
    case NPrefixOp:
    case NPostfixOp:
    case NReturn:
    case NTuple:
    case NTupleType:
    case NTypeCast:
    case NZeroInit:
    case _NodeKindMax:
      CCtxErrorf(u->cc, n->pos, "invalid top-level AST node %s", NodeKindName(n->kind));
      break;
  }
  return false;
}

