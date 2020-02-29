#include "wp.h"
#include "ptrmap.h"

// Lookup table N<kind> => name
static const char* const NodeKindNameTable[] = {
  #define I_ENUM(name, _cls) #name,
  DEF_NODE_KINDS(I_ENUM)
  #undef  I_ENUM
};

// Lookup table N<kind> => NClass<class>
const NodeClass NodeClassTable[] = {
  #define I_ENUM(_name, cls) NodeClass##cls,
  DEF_NODE_KINDS(I_ENUM)
  #undef  I_ENUM
};


const char* NodeKindName(NodeKind t) {
  return NodeKindNameTable[t];
}

const char* NodeClassName(NodeClass c) {
  switch (c) {
    case NodeClassInvalid: return "Invalid";
    case NodeClassExpr: return "Expr";
    case NodeClassType: return "Type";
  }
  return "NodeClass?";
}


// NBad node
static const Node _NodeBad = {NULL,NBad,{0,0},NULL};
const Node* NodeBad = &_NodeBad;


Node* NodeAlloc(NodeKind kind) {
  auto n = (Node*)calloc(1, sizeof(Node));
  n->kind = kind;
  if (kind == NBlock || kind == NList || kind == NFile) {
    n->u.array.a.v = n->u.array.astorage;
    n->u.array.a.cap = countof(n->u.array.astorage);
  }
  return n;
}


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


static Str reprEmpty(Str s, ReprCtx* ctx) {
  s = indent(s, ctx->ind);
  return sdscatlen(s, "()", 2);
}


static Scope* getScope(const Node* n) {
  switch (n->kind) {
    case NBlock:
    case NList:
    case NFile:
      return n->u.array.scope;
    case NFun:
      return n->u.fun.scope;
    default:
      return NULL;
  }
}


static Str nodeRepr(const Node* n, Str s, ReprCtx* ctx) {
  assert(n);

  // dlog("nodeRepr %s", NodeKindNameTable[n->kind]);
  // if (n->kind == NIdent) {
  //   dlog("  addr:   %p", n);
  //   dlog("  name:   %s", n->u.ref.name);
  //   if (n->u.ref.target == NULL) {
  //     dlog("  target: <null>");
  //   } else {
  //     dlog("  target:");
  //     dlog("    addr:   %p", n->u.ref.target);
  //     dlog("    kind:   %s", NodeKindNameTable[n->u.ref.target->kind]);
  //   }
  // }

  // cycle guard
  if (PtrMapSet(&ctx->seen, (void*)n, (void*)1) != NULL) {
    PtrMapDel(&ctx->seen, (void*)n);
    s = sdscatfmt(s, " [cyclic %s]", NodeKindNameTable[n->kind]);
    return s;
  }

  s = indent(s, ctx->ind);
  s = sdscatfmt(s, "(%s ", NodeKindNameTable[n->kind]);

  ctx->ind += 2;

  if (n->type) {
    auto t = n->type;
    if (t->kind == NType) {
      s = sdscatfmt(s, "<%S> ", t->u.ref.name);
    } else if (t->kind == NIdent) {
      s = sdscatfmt(s, "<~%S> ", t->u.ref.name);
    } else if (t->kind == NBad) {
      s = sdscat(s, "<BAD> ");
    } else {
      s = sdscatlen(s, "<", 1);
      s = nodeRepr(t, s, ctx);
      s = sdscatlen(s, "> ", 2);
    }
  }

  // auto scope = getScope(n);
  // if (scope) {
  //   s = sdscatprintf(s, "[scope %p] ", scope);
  // }

  switch (n->kind) {

  // does not use u
  case NBad:
  case NNone:
  case NZeroInit:
    sdssetlen(s, sdslen(s)-1); // trim away trailing " " from s
    break;

  // uses u.integer
  case NInt:
    s = sdscatfmt(s, "%U", n->u.integer);
    break;
  case NBool:
    if (n->u.integer == 0) {
      s = sdscat(s, "false");
    } else {
      s = sdscat(s, "true");
    }
    break;

  // uses u.real
  case NFloat:
    s = sdscatprintf(s, "%f", n->u.real);
    break;

  // uses u.str
  case NComment:
    s = sdscatrepr(s, (const char*)n->u.str.ptr, n->u.str.len);
    break;

  // uses u.ref
  case NIdent:
    assert(n->u.ref.name);
    s = sdscatsds(s, n->u.ref.name);
    if (n->u.ref.target) {
      s = sdscatfmt(s, " @%s", NodeKindNameTable[n->u.ref.target->kind]);
      // s = nodeRepr(n->u.ref.target, s, ctx);
    }
    break;
  case NType:
    s = sdscatfmt(s, "<%S>", n->u.ref.name);
    break;
  case NFunType: // TODO
    s = sdscat(s, "<TODO-ast.c-NFunType>");
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

  // uses u.array
  case NBlock:
  case NList:
  case NFile:
  {
    sdssetlen(s, sdslen(s)-1); // trim away trailing " " from s
    auto a = &n->u.array.a;
    for (u32 i = 0; i < a->len; i++) {
      s = nodeRepr(a->v[i], s, ctx);
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

    s = sdscatprintf(s, " %p", n);

    if (f->params) {
      s = nodeRepr(f->params, s, ctx);
    } else {
      s = reprEmpty(s, ctx);
    }

    if (f->result) {
      s = nodeRepr(f->result, s, ctx);
    } else {
      s = reprEmpty(s, ctx);
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

  case _NodeKindMax: break;
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


Scope* ScopeNew(const Scope* parent) {
  // TODO: slab alloc together with AST nodes for cache efficiency
  auto s = (Scope*)malloc(sizeof(Scope));
  s->parent = parent;
  s->childcount = 0;
  SymMapInit(&s->bindings, 8);
  return s;
}


static const Scope* globalScope = NULL;


void ScopeFree(Scope* s) {
  SymMapFree(&s->bindings);
  free(s);
}


const Scope* GetGlobalScope() {
  if (globalScope == NULL) {
    auto s = ScopeNew(NULL);

    #define X(name) SymMapSet(&s->bindings, sym_##name, (void*)Type_##name);
    TYPE_SYMS(X)
    #undef X

    #define X(name, _typ, _val) SymMapSet(&s->bindings, sym_##name, (void*)Const_##name);
    PREDEFINED_CONSTANTS(X)
    #undef X

    globalScope = s;
  }
  return globalScope;
}


const Node* ScopeAssoc(Scope* s, Sym key, const Node* value) {
  return SymMapSet(&s->bindings, key, value);
}


const Node* ScopeLookup(const Scope* scope, Sym s) {
  const Node* n = NULL;
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
