// Resolve types in an AST. Usuaully run after Parse() and ResolveSym()
#include "wp.h"
#include "typeid.h"
#include "convlit.h"

#define DEBUG_MODULE "typeres"

#ifdef DEBUG_MODULE
  #define dlog_mod(format, ...) dlog("[" DEBUG_MODULE "] " format, ##__VA_ARGS__)
#else
  #define dlog_mod(...) do{}while(0)
#endif


typedef struct {
  CCtx* cc;
  u32   idealNest;  // resolves ideal types when > 0
  CType lastCType;  // the most recent CType picked in resolveIdealType
} ResCtx;


static Node* resolveType(ResCtx* ctx, Node* n);

void ResolveType(CCtx* cc, Node* n) {
  ResCtx ctx = { cc, 0, 0 };
  n->type = resolveType(&ctx, n);
}


static Node* resolveFunType(ResCtx* ctx, Node* n) {
  Node* ft = NewNode(ctx->cc->mem, NFunType);
  auto result = n->type;

  // Important: To avoid an infinite loop when resolving a function which calls itself,
  // we set the unfinished function type objects ahead of calling resolve.
  n->type = ft;

  if (n->fun.params) {
    resolveType(ctx, n->fun.params);
    ft->t.fun.params = (Node*)n->fun.params->type;
  }

  if (result) {
    ft->t.fun.result = (Node*)resolveType(ctx, result);
  }

  if (n->fun.body) {
    auto bodyType = resolveType(ctx, n->fun.body);
    if (ft->t.fun.result == NULL) {
      ft->t.fun.result = bodyType;
    } else if (!TypeEquals(ft->t.fun.result, bodyType)) {
      CCtxErrorf(ctx->cc, n->fun.body->pos, "cannot use type %s as return type %s",
        NodeReprShort(bodyType), NodeReprShort(ft->t.fun.result));
    }
  }

  n->type = ft;
  return ft;
}


// resolveIdealType resolves types of "untyped" expressions.
//
// Returns NULL to signal "pass through" meaning this function doesn't handle n->kind.
// In this case the caller should traverse the AST at n and call resolveIdealType as needed
// when untyped nodes are found. This simplifies the implementation and avoids duplicate
// traversal code in resolveIdealType and resolveType.
//
static Node* resolveIdealType(ResCtx* ctx, Node* n) {
  assert(n != NULL);
  assert(NodeIsUntyped(n));

  dlog("resolveIdealType n->kind = %s", NodeKindName(n->kind));

  switch (n->kind) {

  case NIntLit:
  case NFloatLit:
    // Note: NBoolLit is always typed
    ctx->lastCType = n->val.ct;
    return n->type = IdealType(n->val.ct);

  case NBinOp:
    switch (n->op.op) {
      case TEq:       // "=="
      case TNEq:      // "!="
      case TLt:       // "<"
      case TLEq:      // "<="
      case TGt:       // ">"
      case TGEq:      // ">="
      case TAndAnd:   // "&&
      case TPipePipe: // "||
        return n->type = Type_bool;

      case TShl:
      case TShr:
        // shifts are always of left (receiver) type
        return n->type = resolveType(ctx, n->op.left);

      default: {
        auto lt  = resolveType(ctx, n->op.left);
        auto lct = ctx->lastCType;
        auto rt  = resolveType(ctx, n->op.right);
        auto rct = ctx->lastCType;
        return n->type = (lct > rct) ? lt : rt;
      }
    }

  default:
    dlog("TODO resolveIdealType kind %s", NodeKindName(n->kind));
    return NULL;
  }
}


static Node* resolveType(ResCtx* ctx, Node* n) {
  assert(n != NULL);

  dlog_mod("resolveType %s %p (%s class) type %s",
    NodeKindName(n->kind),
    n,
    NodeClassName(NodeClassTable[n->kind]),
    NodeReprShort(n->type));

  if (NodeKindIsType(n->kind)) {
    dlog_mod("  => %s", NodeReprShort(n));
    return n;
  }

  if (n->kind == NFun) {
    // type already resolved
    if (n->type && n->type->kind == NFunType) {
      dlog_mod("  => %s", NodeReprShort(n->type));
      return n->type;
    }
  } else if (n->type != NULL) {
    // type already resolved
    if (ctx->idealNest > 0 && n->type == Type_ideal) {
      // n is untyped and idealNest indicates that we are allowed to resolve "ideal" types to
      // concrete types. It's really only constant literals which are actually of ideal type,
      // so switch on those and lower CType to concrete type.
      // In case n is not a constant literal, we simply continue as the AST at n is a compound
      // which contains one or more untyped constants. I.e. continue to traverse AST.
      switch (n->kind) {
        case NIntLit:
        case NFloatLit:
          // Note: NBoolLit is always typed
          return n->type = IdealType(n->val.ct);
        default: break;
      }
      // continue
      n->type = Type_nil;
    } else {
      dlog_mod("  => %s", NodeReprShort(n->type));
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
      resolveType(ctx, n)
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
        ctx->idealNest++;
        n->type = resolveType(ctx, e->node);
        ctx->idealNest--;
        break;
      } else {
        resolveType(ctx, e->node);
        e = e->next;
      }
    }
    // Note: No need to set n->type=Type_nil since that is done already (before the switch.)
    break;
  }

  case NTuple: {
    Node* tt = NewNode(ctx->cc->mem, NTupleType);
    NodeListForEach(&n->array.a, n, {
      auto t = resolveType(ctx, n);
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
    n->type = resolveFunType(ctx, n);
    break;

  // uses u.op
  case NPostfixOp:
  case NPrefixOp:
  case NReturn: {
    ctx->idealNest++;
    n->type = resolveType(ctx, n->op.left);
    ctx->idealNest--;
    break;
  }
  case NBinOp:
  case NAssign: {
    assert(n->op.right != NULL);
    // for the target type of the operation, pick first, in order:
    // 1. use already-resolved type of LHS, else
    // 2. use already-resolved type of RHS, else
    // 3. resolve type of LHS, in concrete mode, and use that.
    //
    // temporarily disable ideal resolution, in case of e.g.
    //   "{ a = 1 ; lastexpr = a + (4 as int16) }"
    // which would run the resolver with idealNest>0 for lastexpr, which would incorrectly
    // cause a to be resolved to "int".
    auto idealNest = ctx->idealNest;
    ctx->idealNest = 0;
    auto lt = resolveType(ctx, n->op.left);
    auto rt = resolveType(ctx, n->op.right);
    ctx->idealNest = idealNest;

    if (lt == rt || (lt != Type_ideal && TypeEquals(lt, rt))) {
      // left and right are of same type and none of them are untyped.
      n->type = lt;
    } else {
      auto t = lt;
      if (t == Type_ideal) {
        t = rt;
        if (t == Type_ideal) {
          assert(ctx->idealNest <= 0); // resolve to ideal must mean idealNest is inactive
          ctx->idealNest++;
          t = resolveType(ctx, n->op.left);
          ctx->idealNest--;
        }
      }
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

  // uses u.call
  case NTypeCast: {
    assert(n->call.receiver != NULL);
    if (!NodeKindIsType(n->call.receiver->kind)) {
      CCtxErrorf(ctx->cc, n->pos,
        "invalid conversion to non-type %s",
        NodeReprShort(n->call.receiver));
      break;
    }
    auto argstype = resolveType(ctx, n->call.args);
    n->type = resolveType(ctx, n->call.receiver);
    if (argstype != NULL && TypeEquals(argstype, n->type)) {
      // eliminate type cast since source is already target type
      memcpy(n, n->call.args, sizeof(Node));
      break;
    }
    // attempt conversion to eliminate type cast
    n->call.args = ConvlitExplicit(ctx->cc, n->call.args, n->call.receiver);
    if (TypeEquals(n->call.args->type, n->type)) {
      memcpy(n, n->call.args, sizeof(Node));
    }
    break;
  }
  case NCall: {
    auto argstype = resolveType(ctx, n->call.args);
    // Note: resolveFunType breaks handles cycles where a function calls itself,
    // making this safe (i.e. will not cause an infinite loop.)
    auto recvt = resolveType(ctx, n->call.receiver);
    assert(recvt != NULL);
    if (recvt->kind != NFunType) {
      CCtxErrorf(ctx->cc, n->pos, "cannot call %s", NodeReprShort(n->call.receiver));
      break;
    }
    // Note: Consider arguments with defaults:
    // fun foo(a, b int, c int = 0)
    // foo(1, 2) == foo(1, 2, 0)
    if (!TypeEquals(recvt->t.fun.params, argstype)) {
      CCtxErrorf(ctx->cc, n->pos, "incompatible arguments %s in function call. Expected %s",
        NodeReprShort(argstype), NodeReprShort(recvt->t.fun.params));
    }
    n->type = recvt->t.fun.result;
    break;
  }

  // uses u.field
  case NLet:
  case NArg:
  case NField: {
    if (n->field.init) {
      n->type = resolveType(ctx, n->field.init);
    } else {
      n->type = Type_nil;
    }
    break;
  }

  // uses u.cond
  case NIf: {
    auto cond = n->cond.cond;
    auto condt = resolveType(ctx, cond);
    if (condt != Type_bool) {
      CCtxErrorf(ctx->cc, cond->pos, "non-bool %s (type %s) used as condition",
        NodeReprShort(cond), NodeReprShort(condt));
    }
    auto thent = resolveType(ctx, n->cond.thenb);
    if (n->cond.elseb) {
      auto elset = resolveType(ctx, n->cond.elseb);
      // branches must be of the same type.

      if (thent == Type_ideal) {
        dlog("thent == Type_ideal");
      }
      if (thent == Type_nil) {
        dlog("thent == Type_nil");
      }

      if (elset == Type_ideal) {
        dlog("elset == Type_ideal");
      }
      if (elset == Type_nil) {
        dlog("elset == Type_nil");
      }

      // TODO: only check type when the result is used.
      if (!TypeEquals(thent, elset)) {
        // attempt implicit cast. E.g.
        //
        // x = 3 as int16 ; y = if true x else 0
        //                              ^      ^
        //                            int16   int
        //
        n->cond.elseb = ConvlitImplicit(ctx->cc, n->cond.elseb, thent);
        if (!TypeEquals(n->cond.elseb->type, elset)) {
          CCtxErrorf(ctx->cc, n->pos, "if..else branches of mixed incompatible types %s %s",
            NodeReprShort(thent), NodeReprShort(elset));
        }
      }
    }
    n->type = thent;
    break;
  }

  case NIdent:
    if (n->ref.target) {
      // NULL when identifier failed to resolve
      n->type = resolveType(ctx, (Node*)n->ref.target);
    }
    break;

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
