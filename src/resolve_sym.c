// Resolve identifiers in an AST. Usuaully run right after parsing.
#include "wp.h"


typedef struct {
  CCtx* cc;
  uint  fnest;  // level of function nesting. 0 at file level
} ResCtx;


static Node* resolve(Node* n, Scope* scope, ResCtx* ctx);


Node* ResolveSym(CCtx* cc, Node* n, Scope* scope) {
  ResCtx ctx = { cc, 0 };
  return resolve(n, scope, &ctx);
}


static void resolveErrorf(ResCtx* ctx, SrcPos pos, const char* format, ...) {
  if (ctx->cc->errh == NULL) {
    return;
  }
  va_list ap;
  va_start(ap, format);
  auto msg = sdsempty();
  if (strlen(format) > 0) {
    msg = sdscatvprintf(msg, format, ap);
    assert(sdslen(msg) > 0); // format may contain %S which is not supported by sdscatvprintf
  }
  va_end(ap);
  ctx->cc->errh(&ctx->cc->src, pos, msg, ctx->cc->userdata);
  sdsfree(msg);
}


static inline Node* resolveConst(Node* n, Scope* scope, ResCtx* ctx) {
  assert(n->u.field.init != NULL);
  auto value = resolve(n->u.field.init, scope, ctx);
  // short-circuit constants inside functions
  if (value != n) {
    if (ctx->fnest > 0) {
      return value;
    }
    n->u.field.init = value;
  }
  return n;
}


static const Node* resolveIdent(const Node* n, Scope* scope, ResCtx* ctx) {
  while (1) {
    const Node* target = n->u.ref.target;
    // dlog("resolveIdent %s", n->u.ref.name);
    if (target == NULL) {
      target = ScopeLookup(scope, n->u.ref.name);
      if (target == NULL) {
        resolveErrorf(ctx, n->pos, "undefined symbol %s", n->u.ref.name);
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


static Node* resolve(Node* n, Scope* scope, ResCtx* ctx) {
  // dlog("resolve(%s, scope=%p)", NodeKindName(n->kind), scope);

  if (n->type != NULL && n->type->kind != NType) {
    if (n->kind == NFun) {
      ctx->fnest++;
    }
    n->type = resolve((Node*)n->type, scope, ctx);
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
    NodeListMap(&n->u.array.a, n,
      resolve(n, scope, ctx)
    );
    break;
  }

  // uses u.fun
  case NFun: {
    ctx->fnest++;
    if (n->u.fun.params) {
      resolve(n->u.fun.params, scope, ctx);
    }
    if (n->type) {
      resolve(n->type, scope, ctx);
    }
    auto body = n->u.fun.body;
    if (body) {
      if (n->u.fun.scope) {
        scope = n->u.fun.scope;
      }
      resolve(body, scope, ctx);
    }
    ctx->fnest--;
    break;
  }

  // uses u.op
  case NOp:
  case NPrefixOp:
  case NAssign:
  case NReturn:
    n->u.op.left = resolve(n->u.op.left, scope, ctx);
    if (n->u.op.right) {
      n->u.op.right = resolve(n->u.op.right, scope, ctx);
    }
    break;

  // uses u.call
  case NCall:
    n->u.call.args = resolve(n->u.call.args, scope, ctx);
    n->u.call.receiver = resolve(n->u.call.receiver, scope, ctx);
    break;

  // uses u.field
  case NVar:
  case NField: {
    if (n->u.field.init) {
      n->u.field.init = resolve(n->u.field.init, scope, ctx);
    }
    break;
  }
  case NConst:
    return resolveConst(n, scope, ctx);

  // uses u.cond
  case NIf:
    resolve(n->u.cond.cond, scope, ctx);
    resolve(n->u.cond.thenb, scope, ctx);
    if (n->u.cond.elseb) {
      resolve(n->u.cond.elseb, scope, ctx);
    }
    break;

  default:
    break;

  } // switch n->kind

  return n;
}

