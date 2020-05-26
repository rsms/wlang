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
  assert(n->field.init != NULL);
  auto value = resolve(n->field.init, scope, ctx);
  // short-circuit constants inside functions
  if (value != n) {
    if (ctx->fnest > 0) {
      return value;
    }
    n->field.init = value;
  }
  return n;
}


static const Node* resolveIdent(const Node* n, Scope* scope, ResCtx* ctx) {
  while (1) {
    const Node* target = n->ref.target;
    // dlog("resolveIdent %s", n->ref.name);
    if (target == NULL) {
      target = ScopeLookup(scope, n->ref.name);
      if (target == NULL) {
        resolveErrorf(ctx, n->pos, "undefined symbol %s", n->ref.name);
        ((Node*)n)->ref.target = NodeBad;
        return n;
      }
      ((Node*)n)->ref.target = target;
      // dlog("resolveIdent %s => %s", n->ref.name, NodeKindName(target->kind));
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

  if (n->type != NULL && n->type->kind != NBasicType) {
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
  case NTuple:
  case NFile: {
    if (n->array.scope) {
      scope = n->array.scope;
    }
    NodeListMap(&n->array.a, n,
      resolve(n, scope, ctx)
    );
    break;
  }

  // uses u.fun
  case NFun: {
    ctx->fnest++;
    if (n->fun.params) {
      resolve(n->fun.params, scope, ctx);
    }
    if (n->type) {
      resolve(n->type, scope, ctx);
    }
    auto body = n->fun.body;
    if (body) {
      if (n->fun.scope) {
        scope = n->fun.scope;
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
  case NReturn: {
    auto newleft = resolve(n->op.left, scope, ctx);
    if (n->kind != NAssign || n->op.left->kind != NIdent) {
      // note: in case of assignment where the left side is an identifier,
      // avoid replacing the identifier with its value.
      // This branch is taken in all other cases.
      n->op.left = newleft;
    }
    if (n->op.right) {
      n->op.right = resolve(n->op.right, scope, ctx);
    }
    break;
  }

  // uses u.call
  case NCall:
    n->call.args = resolve(n->call.args, scope, ctx);
    n->call.receiver = resolve(n->call.receiver, scope, ctx);
    break;

  // uses u.field
  case NVar:
  case NLet:
  case NField: {
    if (n->field.init) {
      n->field.init = resolve(n->field.init, scope, ctx);
    }
    break;
  }
  case NConst:
    return resolveConst(n, scope, ctx);

  // uses u.cond
  case NIf:
    resolve(n->cond.cond, scope, ctx);
    resolve(n->cond.thenb, scope, ctx);
    if (n->cond.elseb) {
      resolve(n->cond.elseb, scope, ctx);
    }
    break;

  case NNone:
  case NBad:
  case NBasicType:
  case NFunType:
  case NTupleType:
  case NComment:
  case NNil:
  case NBool:
  case NInt:
  case NFloat:
  case NZeroInit:
  case _NodeKindMax:
    break;

  } // switch n->kind

  return n;
}

