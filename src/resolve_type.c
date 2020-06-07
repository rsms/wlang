// Resolve types in an AST. Usuaully run after Parse() and ResolveSym()
#include "wp.h"
#include "typeid.h"
#include "convlit.h"

// #define DEBUG_MODULE "typeres"

#ifdef DEBUG_MODULE
  #define dlog_mod(format, ...) dlog("[" DEBUG_MODULE "] " format, ##__VA_ARGS__)
#else
  #define dlog_mod(...) do{}while(0)
#endif


typedef enum RFlag {
  RFlagNone = 0,
  RFlagExplicitTypeCast = 1 << 0,
} RFlag;


typedef struct {
  CCtx* cc;
  Array reqestedTypeStack; TypeCode* reqestedTypeStackStorage[4];
  bool  explicitTypeCast;
} ResCtx;


static Node* resolveType(ResCtx* ctx, Node* n, RFlag fl);

void ResolveType(CCtx* cc, Node* n) {
  ResCtx ctx = { cc, {0}, {0}, false };
  ArrayInitWithStorage(
    &ctx.reqestedTypeStack,
    ctx.reqestedTypeStackStorage,
    countof(ctx.reqestedTypeStackStorage));

  n->type = resolveType(&ctx, n, RFlagNone);

  ArrayFree(&ctx.reqestedTypeStack, cc->mem);
}


inline static Node* requestedType(ResCtx* ctx) {
  if (ctx->reqestedTypeStack.len > 0) {
    return (Node*)ctx->reqestedTypeStack.v[ctx->reqestedTypeStack.len - 1];
  }
  return NULL;
}

inline static void requestedTypePush(ResCtx* ctx, Node* t) {
  assert(NodeIsType(t));
  dlog_mod("push requestedType %s", fmtnode(t));
  ArrayPush(&ctx->reqestedTypeStack, t, ctx->cc->mem);
}

inline static void requestedTypePop(ResCtx* ctx) {
  assert(ctx->reqestedTypeStack.len > 0);
  dlog_mod("pop requestedType %s", fmtnode(requestedType(ctx)));
  ArrayPop(&ctx->reqestedTypeStack);
}


static Node* resolveFunType(ResCtx* ctx, Node* n, RFlag fl) {
  Node* ft = NewNode(ctx->cc->mem, NFunType);
  auto result = n->type;

  // Important: To avoid an infinite loop when resolving a function which calls itself,
  // we set the unfinished function type objects ahead of calling resolve.
  n->type = ft;

  if (n->fun.params) {
    resolveType(ctx, n->fun.params, fl);
    ft->t.fun.params = (Node*)n->fun.params->type;
  }

  if (result) {
    ft->t.fun.result = (Node*)resolveType(ctx, result, fl);
  }

  if (n->fun.body) {
    auto bodyType = resolveType(ctx, n->fun.body, fl);
    if (ft->t.fun.result == NULL) {
      ft->t.fun.result = bodyType;
    } else if (!TypeEquals(ft->t.fun.result, bodyType)) {
      CCtxErrorf(ctx->cc, n->fun.body->pos, "cannot use type %s as return type %s",
        fmtnode(bodyType), fmtnode(ft->t.fun.result));
    }
  }

  n->type = ft;
  return ft;
}


static Node* resolveIdealType(ResCtx* ctx, Node* n, RFlag fl) {
  // lower ideal types in all cases but NLet
  Node* reqtype = requestedType(ctx);
  if (reqtype != NULL) {
    dlog_mod("resolve ideal type of %s to %s", fmtnode(n), fmtnode(reqtype));
  } else {
    dlog_mod("resolve ideal type of %s to default", fmtnode(n));
  }
  // It's really only constant literals which are actually of ideal type, so switch on those
  // and lower CType to concrete type.
  // In case n is not a constant literal, we simply continue as the AST at n is a compound
  // which contains one or more untyped constants. I.e. continue to traverse AST.
  switch (n->kind) {
    case NIntLit:
    case NFloatLit:
      if (reqtype != NULL) {
        auto n2 = convlit(ctx->cc, n, reqtype, /*explicit*/(fl & RFlagExplicitTypeCast));
        // dlog("ConvlitImplicit %s (type %s) => %s", fmtnode(n), fmtnode(reqtype), fmtnode(n2));
        if (n2 != n) {
          memcpy(n, n2, sizeof(Node));
        }
      } else {
        n->type = IdealType(n->val.ct);
      }
      break;

    case NBoolLit:
      // always typed; should never be ideal
      assert(0 && "BoolLit with ideal type");
      break;

    default:
      dlog("TODO ideal type for n->kind=%s", NodeKindName(n->kind));
      break;
  }
  return n->type;
}


static Node* resolveType(ResCtx* ctx, Node* n, RFlag fl) {
  assert(n != NULL);

  dlog_mod("resolveType %s %p (%s class) type %s",
    NodeKindName(n->kind),
    n,
    NodeClassName(NodeClassTable[n->kind]),
    fmtnode(n->type));

  if (NodeKindIsType(n->kind)) {
    dlog_mod("  => %s", fmtnode(n));
    return n;
  }

  if (n->kind == NFun) {
    // type already resolved
    if (n->type && n->type->kind == NFunType) {
      dlog_mod("  => %s", fmtnode(n->type));
      return n->type;
    }
  } else if (n->type != NULL) {
    // Has type already. Constant literals might have ideal type.
    if (n->kind != NLet && n->type == Type_ideal) {
      n->type = resolveIdealType(ctx, n, fl);
      if (n->type == Type_ideal) {
        n->type = Type_nil;
        // continue
      } else {
        // resolve successful
        return n->type;
      }
    } else {
      // already typed
      dlog_mod("  => %s", fmtnode(n->type));
      return n->type;
    }
  } else {
    // Set type to nil here to break any self-referencing cycles.
    // NFun is special-cased as it stores result type in n->type. Note that resolveFunType
    // handles breaking of cycles.
    // A nice side effect of this is that for error cases and nodes without types, the
    // type "defaults" to nil and we can avoid setting Type_nil in the switch below.
    n->type = Type_nil;
  }

  // branch on node kind
  switch (n->kind) {

  // uses u.array
  case NFile:
    n->type = Type_nil;
    NodeListForEach(&n->array.a, n,
      resolveType(ctx, n, fl)
    );
    break;

  case NBlock: {
    // type of a block is the type of the last expression.
    auto e = n->array.a.head;
    while (e != NULL) {
      if (e->next == NULL) {
        // Last node, in which case we set the flag to resolve literals
        // so that implicit return values gets properly typed.
        // This also becomes the type of the block.
        n->type = resolveType(ctx, e->node, fl);
        break;
      } else {
        resolveType(ctx, e->node, fl);
        e = e->next;
      }
    }
    // Note: No need to set n->type=Type_nil since that is done already (before the switch.)
    break;
  }

  case NTuple: {
    Node* tt = NewNode(ctx->cc->mem, NTupleType);
    NodeListForEach(&n->array.a, n, {
      auto t = resolveType(ctx, n, fl);
      if (!t) {
        t = (Node*)NodeBad;
        CCtxErrorf(ctx->cc, n->pos, "unknown type");
      }
      NodeListAppend(ctx->cc->mem, &tt->t.tuple, t);
    });
    n->type = tt;
    break;
  }

  // uses u.fun
  case NFun:
    n->type = resolveFunType(ctx, n, fl);
    break;

  // uses u.op
  case NPostfixOp:
  case NPrefixOp:
  case NReturn: {
    n->type = resolveType(ctx, n->op.left, fl);
    break;
  }
  case NBinOp:
  case NAssign: {
    assert(n->op.right != NULL);
    // for the target type of the operation, pick first, in order:
    // 1. use already-resolved type of LHS, else
    // 2. use already-resolved type of RHS, else
    // 3. resolve type of LHS, in concrete mode, and use that.
    auto lt = resolveType(ctx, n->op.left, fl);
    auto rt = resolveType(ctx, n->op.right, fl);
    if (lt == rt || TypeEquals(lt, rt)) {
      // left and right are of same type and none of them are untyped.
      n->type = lt;
    } else {
      auto t = lt != NULL ? lt : rt;
      auto n2 = ConvlitImplicit(ctx->cc, n, t);
      if (n2 != n) {
        // replace node
        assert(n2->type != NULL);
        memcpy(n, n2, sizeof(Node));
      } else {
        n->type = t;
      }
    }
    break;
  }


  case NTypeCast: {
    assert(n->call.receiver != NULL);
    if (!NodeKindIsType(n->call.receiver->kind)) {
      CCtxErrorf(ctx->cc, n->pos, "invalid conversion to non-type %s", fmtnode(n->call.receiver));
      break;
    }

    fl |= RFlagExplicitTypeCast;

    n->type = resolveType(ctx, n->call.receiver, fl);
    requestedTypePush(ctx, n->type);

    auto argstype = resolveType(ctx, n->call.args, fl);
    if (argstype != NULL && TypeEquals(argstype, n->type)) {
      // eliminate type cast since source is already target type
      memcpy(n, n->call.args, sizeof(Node));
    } else {
      // attempt conversion to eliminate type cast
      n->call.args = ConvlitExplicit(ctx->cc, n->call.args, n->call.receiver);
      if (TypeEquals(n->call.args->type, n->type)) {
        memcpy(n, n->call.args, sizeof(Node));
      }
    }

    requestedTypePop(ctx);
    break;
  }


  case NCall: {
    auto argstype = resolveType(ctx, n->call.args, fl);
    // Note: resolveFunType breaks handles cycles where a function calls itself,
    // making this safe (i.e. will not cause an infinite loop.)
    auto recvt = resolveType(ctx, n->call.receiver, fl);
    assert(recvt != NULL);
    if (recvt->kind != NFunType) {
      CCtxErrorf(ctx->cc, n->pos, "cannot call %s", fmtnode(n->call.receiver));
      break;
    }
    // Note: Consider arguments with defaults:
    // fun foo(a, b int, c int = 0)
    // foo(1, 2) == foo(1, 2, 0)
    if (!TypeEquals(recvt->t.fun.params, argstype)) {
      CCtxErrorf(ctx->cc, n->pos, "incompatible arguments %s in function call. Expected %s",
        fmtnode(argstype), fmtnode(recvt->t.fun.params));
    }
    n->type = recvt->t.fun.result;
    break;
  }

  // uses u.field
  case NLet:
  case NArg:
  case NField: {
    if (n->field.init) {
      n->type = resolveType(ctx, n->field.init, fl);
    } else {
      n->type = Type_nil;
    }
    break;
  }

  // uses u.cond
  case NIf: {
    auto cond = n->cond.cond;
    auto condt = resolveType(ctx, cond, fl);
    if (condt != Type_bool) {
      CCtxErrorf(ctx->cc, cond->pos, "non-bool %s (type %s) used as condition",
        fmtnode(cond), fmtnode(condt));
    }
    auto thent = resolveType(ctx, n->cond.thenb, fl);
    if (n->cond.elseb) {
      requestedTypePush(ctx, thent);
      auto elset = resolveType(ctx, n->cond.elseb, fl);
      requestedTypePop(ctx);
      // branches must be of the same type
      if (!TypeEquals(thent, elset)) {
        // attempt implicit cast. E.g.
        //
        // x = 3 as int16 ; y = if true x else 0
        //                              ^      ^
        //                            int16   int
        //
        n->cond.elseb = ConvlitImplicit(ctx->cc, n->cond.elseb, thent);
        if (!TypeEquals(thent, n->cond.elseb->type)) {
          CCtxErrorf(ctx->cc, n->pos, "if..else branches of mixed incompatible types %s %s",
            fmtnode(thent), fmtnode(elset));
        }
      }
    }
    n->type = thent;
    break;
  }

  case NIdent: {
    auto target = n->ref.target;
    if (target == NULL) {
      // identifier failed to resolve
      break;
    }

    if (target->type == Type_ideal) {
      // identifier names a let binding to an untyped constant expression.
      // Replace the NIdent node with a copy of the constant expression node and resolve its
      // type to a concrete type.
      assert(target->kind == NLet);
      assert(target->field.init != NULL); // let always has init
      assert(NodeIsConst(target->field.init)); // only constants can be of "ideal" type
      memcpy(n, target->field.init, sizeof(Node)); // convert n to copy of value of let binding.
      resolveIdealType(ctx, n, fl);
      break;
    }

    n->type = resolveType(ctx, target, fl);
    break;
  }

  case NBoolLit:
  case NIntLit:
  case NFloatLit:
  case NBad:
  case NNil:
  case NComment:
  case NFunType:
  case NNone:
  case NBasicType:
  case NTupleType:
  case NZeroInit:
  case _NodeKindMax:
    assert(0 && "expected to be typed");
    break;

  }  // switch (n->kind)

  return n->type;
}

