#include "wp.h"

static const char* const NodeKindNames[] = {
  #define I_ENUM(name) #name,
  DEF_NODE_KINDS_EXPR(I_ENUM)
  #undef  I_ENUM

  #define I_ENUM(name) #name,
  DEF_NODE_KINDS(I_ENUM)
  #undef  I_ENUM
};


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


static Str NodeReprInd(const Node* n, Str s, int ind) {
  assert(n);
  s = indent(s, ind);
  s = sdscatfmt(s, "(%s ", NodeKindNames[n->kind]);
  // s = sdscatfmt(s, "(#%u ", n->kind);

  if (n->type) {
    if (n->type->kind == NIdent) {
      s = sdscatfmt(s, "<%S> ", n->type->u.ref.name);
    } else {
      s = sdscatlen(s, "<", 1);
      s = NodeReprInd(n->type, s, 0);
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
    // TODO: cycle guard; target may be cyclic
    // if (n->u.ref.target) {
    //   s = NodeReprInd(n->u.ref.target, s, ind + 2);
    // }
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
    s = NodeReprInd(n->u.op.left, s, ind + 2);
    if (n->u.op.right) {
      s = NodeReprInd(n->u.op.right, s, ind + 2);
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
      s = NodeReprInd(n2, s, ind + 2);
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
      s = NodeReprInd(f->init, s, ind + 2);
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
      s = NodeReprInd(f->params, s, ind + 2);
    } else {
      s = reprEmpty(s, ind + 2);
    }
    if (f->body) {
      s = NodeReprInd(f->body, s, ind + 2);
    }
    break;
  }

  // uses u.call
  case NCall:
    s = NodeReprInd(n->u.call.receiver, s, ind + 2);
    s = NodeReprInd(n->u.call.args, s, ind + 2);
    break;

  // uses u.cond
  case NIf:
    s = NodeReprInd(n->u.cond.cond, s, ind + 2);
    s = NodeReprInd(n->u.cond.thenb, s, ind + 2);
    if (n->u.cond.elseb) {
      s = NodeReprInd(n->u.cond.elseb, s, ind + 2);
    }
    break;

  // Note: No default case, so that the compiler warns us about missing cases.

  }
  s = sdscatlen(s, ")", 1);
  return s;
}


Str NodeRepr(const Node* n, Str s) {
  return NodeReprInd(n, s, 0);
}


const char* NodeKindName(NodeKind t) {
  return NodeKindNames[t];
}


Scope* ScopeNew(u32 nbuckets) {
  // TODO: slab alloc together with AST nodes for cache efficiency
  auto s = (Scope*)malloc(sizeof(Scope));
  s->parent = NULL;
  s->childcount = 0;
  SymMapInit(&s->bindings, nbuckets);
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
