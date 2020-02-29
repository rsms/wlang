// Resolve types in an AST. Usuaully run after Parse() and ResolveSym()
#include "wp.h"


typedef struct {
  Source*       src;
  ErrorHandler* errh;
  void*         userdata;
} ResCtx;


static void resolveType(CCtx* cc, Node* n);


void ResolveType(CCtx* cc, Node* n) {
  resolveType(cc, n);
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


static void resolveFunType(CCtx* cc, Node* n) {
  if (n->u.fun.params) {
    resolveType(cc, n->u.fun.params);
  }
  if (n->u.fun.result) {
    resolveType(cc, n->u.fun.result);
  }

  // Node* ft =  TODO: pick up here when the node allocator is ready

  if (n->u.fun.body) {
    resolveType(cc, n->u.fun.body);
  }
}


static void resolveType(CCtx* cc, Node* n) {
  auto nc = NodeClassTable[n->kind];
  dlog("resolveType %s %s", NodeKindName(n->kind), NodeClassName(nc));

  if (n->type != NULL && NodeClassTable[n->type->kind] == NodeClassType) {
    // type already known
    dlog("  already resolved kind=%s class=%s", NodeKindName(n->type->kind),
      NodeClassName(NodeClassTable[n->type->kind]));
    return;
  }

  //TODO NodeClassTable and NodeClassName for type


  switch (n->kind) {

  // uses u.array
  case NBlock:
  case NList:
  case NFile: {
    // TODO: Type.
    // - For file, type is ignored
    // - For block, type is the last expression (remember to check returns)
    // - For list, type is heterogeneous (tuple)
    auto a = &n->u.array.a;
    for (u32 i = 0; i < a->len; i++) {
      resolveType(cc, a->v[i]);
    }
    break;
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
  case NCall:
    return resolveType(cc, n->u.call.args);
    // Important: we do not resolve type of receiver since it will be a function
    // since ResolveSym has been run already. Trying to resolve the receiver will
    // cause an infinite loop.
    //resolveType(cc, n->u.call.receiver);

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
  // case NZeroInit:
    break;

  }  // switch (n->kind)
}
