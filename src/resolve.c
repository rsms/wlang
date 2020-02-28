// Resolve identifiers in an AST. Usuaully run right after parsing.
#include "wp.h"


typedef struct {
  Source*       src;
  ErrorHandler* errh;
  void*         userdata;
  uint          fnest;  // level of function nesting. 0 at file level
} ResolveCtx;


static Node* _resolve(Node* n, Scope* scope, ResolveCtx* ctx);


Node* Resolve(Node* n, Source* src, ErrorHandler* errh, void* userdata) {
  ResolveCtx ctx = { src, errh, userdata, 0 };
  return _resolve(n, NULL, &ctx);
}


static void resolveErrorf(ResolveCtx* ctx, SrcPos pos, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  auto msg = sdsempty();
  if (strlen(format) > 0) {
    msg = sdscatvprintf(msg, format, ap);
    assert(sdslen(msg) > 0); // format may contain %S which is not supported by sdscatvprintf
  }
  va_end(ap);
  ctx->errh(ctx->src, pos, msg, ctx->userdata);
  sdsfree(msg);
}


static inline Node* resolveConst(Node* n, Scope* scope, ResolveCtx* ctx) {
  assert(n->u.field.init != NULL);
  auto value = _resolve(n->u.field.init, scope, ctx);
  // short-circuit constants inside functions
  if (value != n) {
    if (ctx->fnest > 0) {
      return value;
    }
    n->u.field.init = value;
  }
  return n;
}


static const Node* resolveIdent(const Node* n, Scope* scope, ResolveCtx* ctx) {
  while (1) {
    const Node* target = n->u.ref.target;
    // dlog("resolveIdent %s", n->u.ref.name);
    if (target == NULL) {
      target = ScopeLookup(scope, n->u.ref.name);
      if (target == NULL) {
        if (ctx->errh) {
          resolveErrorf(ctx, n->pos, "undefined symbol %s", n->u.ref.name);
        }
        ((Node*)n)->u.ref.target = NodeBad;
        return n;
      }
      ((Node*)n)->u.ref.target = target;
      // dlog("resolveIdent %s => %s", n->u.ref.name, NodeKindName(target->kind));
    }
    switch (target->kind) {
      case NIdent:
        n = target;
        break; // continue loop
      case NConst:
        return resolveConst((Node*)target, scope, ctx);
      default:
        return target;
    }
  }
}


static Node* _resolve(Node* n, Scope* scope, ResolveCtx* ctx) {
  // dlog("_resolve(%s, scope=%p)", NodeKindName(n->kind), scope);

  if (n->type != NULL && n->type->kind != NType) {
    if (n->kind == NFun) {
      ctx->fnest++;
    }
    n->type = _resolve((Node*)n->type, scope, ctx);
    if (n->kind == NFun) {
      ctx->fnest--;
    }
  }

  switch (n->kind) {

  // uses u.ref
  case NIdent: {
    return (Node*)resolveIdent(n, scope, ctx);
  }

  // uses u.array
  case NBlock:
  case NList:
  case NFile: {
    if (n->u.array.scope) {
      scope = n->u.array.scope;
    }
    // TODO: Type.
    // - For file, type is ignored
    // - For block, type is the last expression (remember to check returns)
    // - For list, type is heterogeneous (build tuple-type)
    // Node* elemType = NULL; ...
    auto a = &n->u.array.a;
    for (u32 i = 0; i < a->len; i++) {
      a->v[i] = _resolve(a->v[i], scope, ctx);
    }
    break;
  }

  // uses u.fun
  case NFun: {
    ctx->fnest++;
    if (n->u.fun.params != NULL) {
      _resolve(n->u.fun.params, scope, ctx);
    }
    auto body = n->u.fun.body;
    if (body) {
      if (n->u.fun.scope) {
        scope = n->u.fun.scope;
      }
      _resolve(body, scope, ctx);
    }
    ctx->fnest--;
    break;
  }

  // uses u.op
  case NOp:
  case NPrefixOp:
  case NAssign:
  case NReturn:
    n->u.op.left = _resolve(n->u.op.left, scope, ctx);
    if (n->u.op.right) {
      n->u.op.right = _resolve(n->u.op.right, scope, ctx);
    }
    break;

  // uses u.call
  case NCall:
    n->u.call.args = _resolve(n->u.call.args, scope, ctx);
    n->u.call.receiver = _resolve(n->u.call.receiver, scope, ctx);
    break;

  // uses u.field
  case NVar:
  case NField: {
    if (n->u.field.init) {
      n->u.field.init = _resolve(n->u.field.init, scope, ctx);
    }
    break;
  }
  case NConst:
    return resolveConst(n, scope, ctx);

  // uses u.cond
  case NIf:
    _resolve(n->u.cond.cond, scope, ctx);
    _resolve(n->u.cond.thenb, scope, ctx);
    if (n->u.cond.elseb) {
      _resolve(n->u.cond.elseb, scope, ctx);
    }
    break;

  case NBool:
  case NInt:
  case NFloat:
  case NZeroInit:
  case NType:
  case NComment:
  case NBad:
  case NNone:
    break;

  } // switch n->kind

  return n;
}

