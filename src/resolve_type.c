// Resolve types in an AST. Usuaully run after Parse() and ResolveSym()
#include "wp.h"
#include "typeid.h"
#include "convlit.h"

// #define DEBUG_TYPE_RESOLUTION 1

// CFlag describes the context of resolution
typedef enum CFlag {
  CFlagNone = 0,
  CFlagLit  = 1 << 0, // literal resolution deferred
} CFlag;


static Node* resolveType(CCtx* cc, Node* n, CFlag fl);


void ResolveType(CCtx* cc, Node* n) {
  n->type = resolveType(cc, n, CFlagNone);
}


static Node* resolveFunType(CCtx* cc, Node* n, CFlag fl) {
  Node* ft = NewNode(cc->mem, NFunType);
  auto result = n->type;

  // Important: To avoid an infinite loop when resolving a function which calls itself,
  // we set the unfinished function type objects ahead of calling resolve.
  n->type = ft;

  if (n->fun.params) {
    resolveType(cc, n->fun.params, fl);
    ft->t.fun.params = (Node*)n->fun.params->type;
  }

  if (result) {
    ft->t.fun.result = (Node*)resolveType(cc, result, fl);
  }

  if (n->fun.body) {
    auto bodyType = resolveType(cc, n->fun.body, fl);
    if (ft->t.fun.result == NULL) {
      ft->t.fun.result = bodyType;
    } else if (!TypeEquals(ft->t.fun.result, bodyType)) {
      CCtxErrorf(cc, n->fun.body->pos, "cannot use type %s as return type %s",
        NodeReprShort(bodyType), NodeReprShort(ft->t.fun.result));
    }
  }

  n->type = ft;
  return ft;
}


// static Node* resolveBinOpType(CCtx* cc, Node* n, Tok op, Node* ltype, Node* rtype) {
//   switch (op) {
//     case TGt:   // >
//     case TLt:   // <
//     case TEq:   // ==
//     case TNEq:  // !=
//     case TLEq:  // <=
//     case TGEq:  // >=
//       return Type_bool;
//     case TPlus:  // +
//     case TMinus: // -
//     case TSlash: // /
//     case TStar:  // *
//       return rtype;
//     default:
//       dlog("TODO resolveBinOpType %s", TokName(op));
//       return rtype;
//   }
// }


static Node* resolveType(CCtx* cc, Node* n, CFlag fl) {
  assert(n != NULL);

  #if DEBUG_TYPE_RESOLUTION
  auto nc = NodeClassTable[n->kind];
  dlog("resolveType fl=%d %s %p (%s class) type %s",
    fl,
    NodeKindName(n->kind),
    n,
    NodeClassName(nc),
    NodeReprShort(n->type));
  #endif

  if (NodeKindIsType(n->kind)) {
    #if DEBUG_TYPE_RESOLUTION
    dlog("  => %s", NodeReprShort(n));
    #endif
    return n;
  }

  if (n->kind == NFun) {
    // type already resolved
    if (n->type && n->type->kind == NFunType) {
      #if DEBUG_TYPE_RESOLUTION
      dlog("  => %s", NodeReprShort(n->type));
      #endif
      return n->type;
    }
  } else if (n->type != NULL) {
    // type already resolved
    #if DEBUG_TYPE_RESOLUTION
    dlog("  => %s", NodeReprShort(n->type));
    #endif
    return n->type;
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
      resolveType(cc, n, fl)
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
        n->type = resolveType(cc, e->node, fl | CFlagLit);
        break;
      } else {
        resolveType(cc, e->node, fl);
        e = e->next;
      }
    }
    // Note: No need to set n->type=Type_nil since that is done already (before the switch.)
    break;
  }

  case NTuple: {
    Node* tt = NewNode(cc->mem, NTupleType);
    NodeListForEach(&n->array.a, n, {
      auto t = resolveType(cc, n, fl);
      if (!t) {
        t = (Node*)NodeBad;
        CCtxErrorf(cc, n->pos, "unknown type");
      }
      NodeListAppend(cc->mem, &tt->t.tuple, t);
    });
    n->type = tt;
    break;
  }

  // uses u.fun
  case NFun:
    n->type = resolveFunType(cc, n, fl);
    break;

  // uses u.op
  case NBinOp:
  case NPostfixOp:
  case NPrefixOp:
  case NAssign:
  case NReturn: {
    if (n->op.right == NULL) {
      n->type = resolveType(cc, n->op.left, fl | CFlagLit);
    } else {
      // for the target type of the operation, pick first, in order:
      // 1. already-resolved type of LHS
      // 2. already-resolved type of RHS
      // 3. resolve type of LHS and use that
      Node* t = n->op.left->type;
      if (t == NULL) {
        t = n->op.right->type;
        if (t == NULL) {
          t = resolveType(cc, n->op.left, fl | CFlagLit);
        }
      }
      fl &= ~CFlagLit; // clear literal resolve flag
      resolveType(cc, n->op.left, fl);
      resolveType(cc, n->op.right, fl);
      auto n2 = ConvlitImplicit(cc, n, t);
      if (n2 != NULL && n2 != n) {
        // replace node
        memcpy(n, n2, sizeof(Node));
      }
      assert(n->type != NULL);
    }
    break;
  }

  // uses u.call
  case NTypeCast: {
    assert(n->call.receiver != NULL);
    if (!NodeKindIsType(n->call.receiver->kind)) {
      CCtxErrorf(cc, n->pos, "invalid conversion to non-type %s", NodeReprShort(n->call.receiver));
      break;
    }
    auto argstype = resolveType(cc, n->call.args, fl);
    n->type = resolveType(cc, n->call.receiver, fl);
    if (argstype != NULL && TypeEquals(argstype, n->type)) {
      // eliminate type cast since source is already target type
      memcpy(n, n->call.args, sizeof(Node));
      break;
    }
    // attempt conversion to eliminate type cast
    auto expr = ConvlitExplicit(cc, n->call.args, n->call.receiver);
    if (expr != NULL) {
      memcpy(n, expr, sizeof(Node));
    }
    break;
  }
  case NCall: {
    auto argstype = resolveType(cc, n->call.args, fl);
    // Note: resolveFunType breaks handles cycles where a function calls itself,
    // making this safe (i.e. will not cause an infinite loop.)
    auto recvt = resolveType(cc, n->call.receiver, fl);
    assert(recvt != NULL);
    if (recvt->kind != NFunType) {
      CCtxErrorf(cc, n->pos, "cannot call %s", NodeReprShort(n->call.receiver));
      break;
    }
    // Note: Consider arguments with defaults:
    // fun foo(a, b int, c int = 0)
    // foo(1, 2) == foo(1, 2, 0)
    if (!TypeEquals(recvt->t.fun.params, argstype)) {
      CCtxErrorf(cc, n->pos, "incompatible arguments %s in function call. Expected %s",
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
      n->type = resolveType(cc, n->field.init, fl);
    } else {
      n->type = Type_nil;
    }
    break;
  }

  // uses u.cond
  case NIf: {
    auto cond = n->cond.cond;
    auto condt = resolveType(cc, cond, fl);
    if (condt != Type_bool) {
      CCtxErrorf(cc, cond->pos, "non-bool %s (type %s) used as condition",
        NodeReprShort(cond), NodeReprShort(condt));
    }
    auto thent = resolveType(cc, n->cond.thenb, fl);
    if (n->cond.elseb) {
      auto elset = resolveType(cc, n->cond.elseb, fl);
      // branches must be of the same type.
      // TODO: only check type when the result is used.
      if (!TypeEquals(thent, elset)) {
        CCtxErrorf(cc, n->pos, "if..else branches of incompatible types %s %s",
          NodeReprShort(thent), NodeReprShort(elset));
      }
    }
    n->type = thent;
    break;
  }

  case NIdent:
    if (n->ref.target) {
      // NULL when identifier failed to resolve
      n->type = resolveType(cc, (Node*)n->ref.target, fl);
    }
    break;

  case NBoolLit:
    assert(n->type == Type_bool); // always typed
    break;
  case NIntLit:
  case NFloatLit:
    if (fl & CFlagLit) {
      dlog("resolve literal type");
      n->type = TypeCodeToTypeNode(n->val.t);
    } else {
      n->type = NULL;
    }
    break;

  // all other: does not have children or are types themselves
  case NBad:
  case NNil:
  case NComment:
  case NFunType:
  case NNone:
  case NBasicType:
  case NTupleType:
  case NZeroInit:
  case _NodeKindMax:
    break;

  }  // switch (n->kind)

  return n->type;
}
