#include "wp.h"


typedef struct {
  Source*       src;
  ErrorHandler* errh;
  void*         userdata;
} ResolveCtx;


static void _resolve(Node* n, Scope* scope, ResolveCtx* ctx) {
  // dlog("_resolve(%s, scope=%p)", NodeKindName(n->kind), scope);

  switch (n->kind) {

  // uses u.ref
  case NIdent: {
    if (n->u.ref.target == NULL) {
      n->u.ref.target = ScopeLookup(scope, n->u.ref.name);
      if (n->u.ref.target == NULL && ctx->errh) {
        auto msg = sdscatfmt(sdsempty(), "undefined symbol %S", n->u.ref.name);
        ctx->errh(ctx->src, n->pos, msg, ctx->userdata);
        sdsfree(msg);
      }
    }
    break;
  }

  // uses u.list
  case NBlock:
  case NList:
  case NFile: {
    if (n->u.list.scope) {
      scope = n->u.list.scope;
    }
    auto n2 = n->u.list.head;
    while (n2) {
      _resolve(n2, scope, ctx);
      n2 = n2->link_next;
    }
    break;
  }

  // uses u.fun
  case NFun: {
    auto body = n->u.fun.body;
    if (body) {
      if (n->u.fun.scope) {
        scope = n->u.fun.scope;
      }
      return _resolve(body, scope, ctx);
    }
    break;
  }

  // uses u.op
  case NOp:
  case NPrefixOp:
  case NAssign:
  case NReturn:
    _resolve(n->u.op.left, scope, ctx);
    if (n->u.op.right) {
      return _resolve(n->u.op.right, scope, ctx);
    }
    break;

  // uses u.call
  case NCall:
    _resolve(n->u.call.args, scope, ctx);
    _resolve(n->u.call.receiver, scope, ctx);
    break;

  // uses u.field
  case NVar:
  case NConst:
  case NField: {
    if (n->u.field.init) {
      return _resolve(n->u.field.init, scope, ctx);
    }
    break;
  }

  // uses u.cond
  case NIf:
    _resolve(n->u.cond.cond, scope, ctx);
    _resolve(n->u.cond.thenb, scope, ctx);
    if (n->u.cond.elseb) {
      _resolve(n->u.cond.elseb, scope, ctx);
    }
    break;

  case NBad:
  case NNone:
  case NZeroInit:
  case NComment:
  case NInt:
    break;

  } // switch n->kind

}

void Resolve(Node* n, Source* src, ErrorHandler* errh, void* userdata) {
  ResolveCtx ctx = { src, errh, userdata };
  return _resolve(n, NULL, &ctx);
}

