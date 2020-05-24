// Resolve types in an AST. Usuaully run after Parse() and ResolveSym()
#include "wp.h"

// #define DEBUG_TYPE_RESOLUTION 1


typedef struct {
  Source*       src;
  ErrorHandler* errh;
  void*         userdata;
} ResCtx;


static Node* resolveType(CCtx* cc, Node* n);


void ResolveType(CCtx* cc, Node* n) {
  n->type = resolveType(cc, n);
}


static bool TypeEquals(const Node* t1, const Node* t2) {
  return t1 == t2;
}


static void errorf(CCtx* cc, SrcPos pos, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  auto msg = sdsempty();
  if (strlen(format) > 0) {
    msg = sdscatvprintf(msg, format, ap);
    assert(sdslen(msg) > 0); // format may contain %S which is not supported by sdscatvprintf
  }
  va_end(ap);
  cc->errh(&cc->src, pos, msg, cc->userdata);
  sdsfree(msg);
}


static Node* resolveFunType(CCtx* cc, Node* n) {
  Node* ft = NewNode(&cc->mem, NFunType);
  auto result = n->type;

  // Important: To avoid an infinite loop when resolving a function which calls itself,
  // we set the unfinished function type objects ahead of calling resolve.
  n->type = ft;

  if (n->u.fun.params) {
    resolveType(cc, n->u.fun.params);
    ft->u.tfun.params = (Node*)n->u.fun.params->type;
  }

  if (result) {
    ft->u.tfun.result = (Node*)resolveType(cc, result);
  } else {
    ft->u.tfun.result = Type_nil;
  }

  if (n->u.fun.body) {
    resolveType(cc, n->u.fun.body);
    // TODO: check result type
  }

  n->type = ft;
  return ft;
}


static Node* resolveBinOpType(CCtx* cc, Node* n, Tok op, Node* ltype, Node* rtype) {
  switch (op) {
    case TEqEq:
    case TNEq:
    case TLEq:
    case TGEq:
      return Type_bool;
    case TPlus:
    case TMinus:
    case TSlash:
    case TStar:
      return rtype;
    default:
      dlog("TODO resolveBinOpType %s", TokName(op));
      return rtype;
  }
}


static bool isTyped(const Node* n) {
  if (n->kind == NFun) {
    return n->type && n->type->kind == NFunType;
  }
  return NodeIsType(n->type);
}


static Node* resolveType(CCtx* cc, Node* n) {

  #if DEBUG_TYPE_RESOLUTION
  auto nc = NodeClassTable[n->kind];
  dlog("resolveType %s %s", NodeKindName(n->kind), NodeClassName(nc));
  #endif

  if (NodeIsType(n)) {
    return n;
  }

  if (isTyped(n)) {
    // type already known
    // dlog("  already resolved kind=%s class=%s", NodeKindName(n->type->kind),
    //   NodeClassName(NodeClassTable[n->type->kind]));
    return n->type;
  }

  // Set type to nil here to break any self-referencing cycles.
  // NFun is special-cased as it stores result type in n->type. Note that resolveFunType
  // handles breaking of cycles.
  // A nice side effect of this is that for error cases and nodes without types, the
  // type "defaults" to nil and we can avoid setting Type_nil in the switch below.
  if (n->kind != NFun) {
    n->type = Type_nil;
  }

  // branch on node kind
  switch (n->kind) {

  // uses u.array
  case NFile:
    n->type = Type_nil;
    NodeListForEach(&n->u.array.a, n,
      resolveType(cc, n));
    break;

  case NBlock:
    // type of a block is the type of the last expression.
    // TODO: handle & check "return" expressions.
    NodeListForEach(&n->u.array.a, n,
      resolveType(cc, n));
    if (n->u.array.a.tail) {
      assert(n->u.array.a.tail->node != NULL);
      n->type = n->u.array.a.tail->node->type;
    } else {
      n->type = Type_nil;
    }
    break;

  case NList: {
    Node* tt = NewNode(&cc->mem, NTupleType);
    NodeListForEach(&n->u.array.a, n, {
      auto t = resolveType(cc, n);
      if (!t) {
        t = (Node*)NodeBad;
        errorf(cc, n->pos, "unknown type");
      }
      NodeListAppend(&cc->mem, &tt->u.array.a, t);
    });
    n->type = tt;
    break;
  }

  // uses u.fun
  case NFun:
    n->type = resolveFunType(cc, n);
    break;

  // uses u.op
  case NOp:
  case NPrefixOp:
  case NAssign:
  case NReturn: {
    auto ltype = resolveType(cc, n->u.op.left);
    if (n->u.op.right == NULL) {
      n->type = ltype;
    } else {
      auto rtype = resolveType(cc, n->u.op.right);
      // For binary operations, the operands must be of compatible types.
      // i.e. 1 == 1.0 is an error because float and int are different,
      // but 1.0 == 1.0 and 1 == 1 is ok (float==float and int==int, respectively).
      if (!TypeEquals(ltype, rtype)) {
        errorf(cc, n->pos, "operation %s on incompatible types %s %s",
          TokName(n->u.op.op), NodeReprShort(ltype), NodeReprShort(rtype));
      }
      if (n->kind == NOp) {
        n->type = resolveBinOpType(cc, n, n->u.op.op, ltype, rtype);
      } else {
        n->type = rtype;
      }
    }
    break;
  }

  // uses u.call
  case NCall: {
    auto argstype = resolveType(cc, n->u.call.args);

    // Note: resolveFunType breaks handles cycles where a function calls itself,
    // making this safe (i.e. will not cause an infinite loop.)
    auto ftype = resolveType(cc, n->u.call.receiver);
    assert(ftype != NULL);

    // Input type is at ftype->u.tfun.params
    // Output type is at ftype->u.tfun.result

    dlog("TODO NCall check argstype <> receiver type");
    // Note: Consider arguments with defaults:
    // fun foo(a, b int, c int = 0)
    // foo(1, 2) == foo(1, 2, 0)

    if (!TypeEquals(ftype->u.tfun.params, argstype)) {
      errorf(cc, n->pos, "incompatible arguments %s in function call. Expected %s",
        NodeReprShort(argstype), NodeReprShort(ftype->u.tfun.params));
    }

    n->type = ftype->u.tfun.result;
    break;
  }

  // uses u.field
  case NVar:
  case NLet:
  case NField: {
    if (n->u.field.init) {
      n->type = resolveType(cc, n->u.field.init);
    } else {
      n->type = Type_nil;
    }
    break;
  }

  // uses u.cond
  case NIf: {
    auto cond = n->u.cond.cond;
    auto condt = resolveType(cc, cond);
    if (condt != Type_bool) {
      errorf(cc, cond->pos, "non-bool %s (type %s) used as condition",
        NodeReprShort(cond), NodeReprShort(condt));
    }
    auto thent = resolveType(cc, n->u.cond.thenb);
    if (n->u.cond.elseb) {
      auto elset = resolveType(cc, n->u.cond.elseb);
      // branches must be of the same type.
      // TODO: only check type when the result is used.
      if (!TypeEquals(thent, elset)) {
        errorf(cc, n->pos, "if..else branches of incompatible types %s %s",
          NodeReprShort(thent), NodeReprShort(elset));
      }
    }
    n->type = thent;
    break;
  }

  case NIdent:
    if (n->u.ref.target) {
      // NULL when identifier failed to resolve
      n->type = resolveType(cc, (Node*)n->u.ref.target);
    }
    break;

  // all other: does not have children
  case NNil:
  case NBad:
  case NBool:
  case NComment:
  case NConst:
  case NFloat:
  case NFunType:
  case NInt:
  case NNone:
  case NType:
  case NTupleType:
  case NZeroInit:
  case _NodeKindMax:
    break;

  }  // switch (n->kind)

  return n->type;
}
