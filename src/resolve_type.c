// Resolve types in an AST. Usuaully run after Parse() and ResolveSym()
#include "wp.h"


typedef struct {
  Source*       src;
  ErrorHandler* errh;
  void*         userdata;
} ResCtx;


static Node* resolveType(CCtx* cc, Node* n);


void ResolveType(CCtx* cc, Node* n) {
  n->type = resolveType(cc, n);
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

  if (n->u.fun.params) {
    resolveType(cc, n->u.fun.params);
    ft->u.tfun.params = (Node*)n->u.fun.params->type;
  }

  if (n->type) {
    ft->u.tfun.result = (Node*)resolveType(cc, n->type);
  }

  if (n->u.fun.body) {
    resolveType(cc, n->u.fun.body);
    // TODO: check result type
  }

  n->type = ft;
  return ft;
}


static bool isTyped(const Node* n) {
  if (n->kind == NFun) {
    return n->type && n->type->kind == NFunType;
  }
  return NodeIsType(n->type);
}


static Node* resolveType(CCtx* cc, Node* n) {
  auto nc = NodeClassTable[n->kind];
  dlog("resolveType %s %s", NodeKindName(n->kind), NodeClassName(nc));

  if (NodeIsType(n)) {
    return n;
  }

  if (isTyped(n)) {
    // type already known
    // dlog("  already resolved kind=%s class=%s", NodeKindName(n->type->kind),
    //   NodeClassName(NodeClassTable[n->type->kind]));
    return n->type;
  }

  //TODO NodeClassTable and NodeClassName for type


  switch (n->kind) {

  // uses u.array
  case NBlock:
  case NFile:
    // TODO: Type.
    // - For file, type is ignored
    // - For block, type is the last expression (remember to check returns)
    // - For list, type is heterogeneous (tuple)
    NodeListForEach(&n->u.array.a, n,
      resolveType(cc, n));
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
    return tt;
  }

  // uses u.fun
  case NFun:
    return resolveFunType(cc, n);

  // uses u.op
  case NOp:
  case NPrefixOp:
  case NAssign:
  case NReturn:
    resolveType(cc, n->u.op.left);
    if (n->u.op.right) {
      return resolveType(cc, n->u.op.right);
    }
    break;

  // uses u.call
  case NCall: {
    auto argstype = resolveType(cc, n->u.call.args);
    dlog("TODO check argstype <> receiver type");

    // Important: we do not resolve type of receiver since it will be a function
    // since ResolveSym has been run already. Trying to resolve the receiver will
    // cause an infinite loop.
    //resolveType(cc, n->u.call.receiver);
  }

  // uses u.field
  case NVar:
  case NField: {
    if (n->u.field.init) {
      return resolveType(cc, n->u.field.init);
    }
    break;
  }

  // uses u.cond
  case NIf:
    resolveType(cc, n->u.cond.cond);
    resolveType(cc, n->u.cond.thenb);
    if (n->u.cond.elseb) {
      return resolveType(cc, n->u.cond.elseb);
    }
    break;

  case NIdent:
    n->type = resolveType(cc, (Node*)n->u.ref.target);
    break;

  // all other: does not have children
  default:
  // case NBad:
  // case NBool:
  // case NComment:
  // case NConst:
  // case NFloat:
  // case NFunType:
  // case NIdent:
  // case NInt:
  // case NNone:
  // case NType:
  // case NTupleType:
  // case NZeroInit:
    break;

  }  // switch (n->kind)

  return n->type;
}
