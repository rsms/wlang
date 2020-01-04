#include "wp.h"
#include "ptrmap.h"

static const char* const NodeKindNames[] = {
  #define I_ENUM(name) #name,
  DEF_NODE_KINDS_EXPR(I_ENUM)
  #undef  I_ENUM

  #define I_ENUM(name) #name,
  DEF_NODE_KINDS(I_ENUM)
  #undef  I_ENUM
};


typedef struct {
  int    ind;  // indentation level
  PtrMap seen; // cycle guard
} ReprCtx;


static Str indent(Str s, int ind) {
  if (ind > 0) {
    s = strgrow(s, ind);
    s = sdscatlen(s, "\n", 1);
    s = sdsgrow(s, sdslen(s) + ind, ' ');
  }
  return s;
}


static Str reprEmpty(Str s, int ind) {
  s = indent(s, ind);
  return sdscatlen(s, "()", 2);
}


static Scope* getScope(const Node* n) {
  switch (n->kind) {
    case NBlock:
    case NList:
    case NFile:
      return n->u.list.scope;
    case NFun:
      return n->u.fun.scope;
    default:
      return NULL;
  }
}


static Str nodeRepr(const Node* n, Str s, ReprCtx* ctx) {
  assert(n);

  // cycle guard
  if (PtrMapSet(&ctx->seen, (void*)n, (void*)1) != NULL) {
    PtrMapDel(&ctx->seen, (void*)n);
    // s = indent(s, ctx->ind);
    s = sdscat(s, " [cyclic]");
    return s;
  }

  s = indent(s, ctx->ind);
  s = sdscatfmt(s, "(%s ", NodeKindNames[n->kind]);
  // s = sdscatfmt(s, "(#%u ", n->kind);

  ctx->ind += 2;

  if (n->type) {
    if (n->type->kind == NIdent) {
      s = sdscatfmt(s, "<%S> ", n->type->u.ref.name);
    } else {
      s = sdscatlen(s, "<", 1);
      s = nodeRepr(n->type, s, ctx);
      s = sdscatlen(s, "> ", 2);
    }
  }

  auto scope = getScope(n);
  if (scope) {
    s = sdscatprintf(s, "[scope %p] ", scope);
  }

  switch (n->kind) {

  // does not use u
  case NBad:
  case NNone:
  case NZeroInit:
    sdssetlen(s, sdslen(s)-1); // trim away trailing " " from s
    break;

  // uses u.str
  case NComment:
    s = sdscatrepr(s, (const char*)n->u.str.ptr, n->u.str.len);
    break;

  // uses u.integer
  case NInt:
    s = sdscatfmt(s, "%U", n->u.integer);
    break;

  // uses u.ref
  case NIdent:
    assert(n->u.ref.name);
    s = sdscatsds(s, n->u.ref.name);
    if (n->u.ref.target) {
      s = nodeRepr(n->u.ref.target, s, ctx);
    }
    break;

  // uses u.op
  case NOp:
  case NPrefixOp:
  case NAssign:
  case NReturn:
    if (n->u.op.op != TNil) {
      s = sdscat(s, TokName(n->u.op.op));
      s = sdscatlen(s, " ", 1);
    }
    s = nodeRepr(n->u.op.left, s, ctx);
    if (n->u.op.right) {
      s = nodeRepr(n->u.op.right, s, ctx);
    }
    break;

  // uses u.list
  case NBlock:
  case NList:
  case NFile:
  {
    sdssetlen(s, sdslen(s)-1); // trim away trailing " " from s
    auto n2 = n->u.list.head;
    while (n2) {
      s = nodeRepr(n2, s, ctx);
      n2 = n2->link_next;
    }
    break;
  }

  // uses u.field
  case NVar:
  case NConst:
  case NField:
  {
    auto f = &n->u.field;
    if (f->name) {
      s = sdscatsds(s, f->name);
    } else {
      s = sdscatlen(s, "_", 1);
    }
    if (f->init) {
      s = nodeRepr(f->init, s, ctx);
    }
    break;
  }

  // uses u.fun
  case NFun: {
    auto f = &n->u.fun;
    if (f->name) {
      s = sdscatsds(s, f->name);
    } else {
      s = sdscatlen(s, "_", 1);
    }
    if (f->params) {
      s = nodeRepr(f->params, s, ctx);
    } else {
      s = reprEmpty(s, ctx->ind + 2);
    }
    if (f->body) {
      s = nodeRepr(f->body, s, ctx);
    }
    break;
  }

  // uses u.call
  case NCall:
    s = nodeRepr(n->u.call.receiver, s, ctx);
    s = nodeRepr(n->u.call.args, s, ctx);
    break;

  // uses u.cond
  case NIf:
    s = nodeRepr(n->u.cond.cond, s, ctx);
    s = nodeRepr(n->u.cond.thenb, s, ctx);
    if (n->u.cond.elseb) {
      s = nodeRepr(n->u.cond.elseb, s, ctx);
    }
    break;

  // Note: No default case, so that the compiler warns us about missing cases.

  }

  ctx->ind -= 2;
  PtrMapDel(&ctx->seen, (void*)n);

  s = sdscatlen(s, ")", 1);
  return s;
}


Str NodeRepr(const Node* n, Str s) {
  ReprCtx ctx = { 0 };
  PtrMapInit(&ctx.seen, 32);
  return nodeRepr(n, s, &ctx);
}


const char* NodeKindName(NodeKind t) {
  return NodeKindNames[t];
}


Scope* ScopeNew() {
  // TODO: slab alloc together with AST nodes for cache efficiency
  auto s = (Scope*)malloc(sizeof(Scope));
  s->parent = NULL;
  s->childcount = 0;
  SymMapInit(&s->bindings, 8);
  return s;
}

void ScopeFree(Scope* s) {
  SymMapFree(&s->bindings);
  free(s);
}


// #define DEBUG_LOOKUP

Node* ScopeLookup(Scope* scope, Sym s) {
  Node* n = NULL;
  while (scope && n == NULL) {
    // dlog("[lookup] %s in scope %p(len=%u)", s, scope, scope->bindings.len);
    n = SymMapGet(&scope->bindings, s);
    scope = scope->parent;
  }
  #ifdef DEBUG_LOOKUP
  dlog("lookup %s => %s", s, n == NULL ? "null" : NodeKindName(n->kind));
  #endif
  return n;
}
