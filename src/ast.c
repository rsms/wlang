#include "wp.h"
#include "ptrmap.h"
#include "tstyle.h"

// #define DEBUG_LOOKUP


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
    case NodeClassConst:   return "Const";
    case NodeClassExpr:    return "Expr";
    case NodeClassType:    return "Type";
  }
  return "NodeClass?";
}


// NBad node
static const Node _NodeBad = {NBad,{0,0,0},NULL,{0}};
const Node* NodeBad = &_NodeBad;


typedef struct {
  int    ind;  // indentation level
  int    maxdepth;
  bool   pretty;
  bool   includeTypes;
  PtrMap seen; // cycle guard
} ReprCtx;


static Str indent(Str s, const ReprCtx* ctx) {
  if (ctx->ind > 0) {
    if (ctx->pretty) {
      s = sdscatlen(s, "\n", 1);
      s = sdsgrow(s, sdslen(s) + ctx->ind, ' ');
    } else {
      s = sdscatlen(s, " ", 1);
    }
  }
  return s;
}


static Str reprEmpty(Str s, const ReprCtx* ctx) {
  s = indent(s, ctx);
  return sdscatlen(s, "()", 2);
}


// static Scope* getScope(const Node* n) {
//   switch (n->kind) {
//     case NBlock:
//     case NTuple:
//     case NFile:
//       return n->array.scope;
//     case NFun:
//       return n->fun.scope;
//     default:
//       return NULL;
//   }
// }


static Str nodeRepr(const Node* n, Str s, ReprCtx* ctx, int depth) {
  assert(n);

  // dlog("nodeRepr %s", NodeKindNameTable[n->kind]);
  // if (n->kind == NIdent) {
  //   dlog("  addr:   %p", n);
  //   dlog("  name:   %s", n->ref.name);
  //   if (n->ref.target == NULL) {
  //     dlog("  target: <null>");
  //   } else {
  //     dlog("  target:");
  //     dlog("    addr:   %p", n->ref.target);
  //     dlog("    kind:   %s", NodeKindNameTable[n->ref.target->kind]);
  //   }
  // }

  // s = sdscatprintf(s, "[%p] ", n);

  if (depth > ctx->maxdepth) {
    s = TStyleYellow(s);
    s = sdscat(s, "...");
    s = TStyleNone(s);
    return s;
  }

  // cycle guard
  if (PtrMapSet(&ctx->seen, (void*)n, (void*)1) != NULL) {
    PtrMapDel(&ctx->seen, (void*)n);
    s = sdscatfmt(s, " [cyclic %s]", NodeKindNameTable[n->kind]);
    return s;
  }

  auto isType = NodeIsType(n);
  if (!isType) {
    s = indent(s, ctx);

    if (n->kind != NFile && ctx->includeTypes) {
      s = TStyleBlue(s);
      if (n->type) {
        s = nodeRepr(n->type, s, ctx, depth + 1);
        s = sdscatlen(s, ":", 1);
      } else {
        s = sdscatlen(s, "?:", 2);
      }
      s = TStyleNone(s);
    }
    s = sdscatfmt(s, "(%s ", NodeKindNameTable[n->kind]);
  }

  ctx->ind += 2;

  // auto scope = getScope(n);
  // if (scope) {
  //   s = sdscatprintf(s, "[scope %p] ", scope);
  // }

  switch (n->kind) {

  // does not use u
  case NBad:
  case NNone:
  case NNil:
  case NZeroInit:
    sdssetlen(s, sdslen(s)-1); // trim away trailing " " from s
    break;

  // uses u.integer
  case NInt:
    s = sdscatfmt(s, "%U", n->integer);
    break;
  case NBool:
    if (n->integer == 0) {
      s = sdscat(s, "false");
    } else {
      s = sdscat(s, "true");
    }
    break;

  // uses u.real
  case NFloat:
    s = sdscatprintf(s, "%f", n->real);
    break;

  // uses u.str
  case NComment:
    s = sdscatrepr(s, (const char*)n->str.ptr, n->str.len);
    break;

  // uses u.ref
  case NIdent:
    s = TStyleRed(s);
    s = sdscatsds(s, n->ref.name);
    s = TStyleNone(s);
    if (n->ref.target) {
      s = sdscatfmt(s, " @%s", NodeKindNameTable[n->ref.target->kind]);
      // s = nodeRepr(n->ref.target, s, ctx, depth + 1);
    }
    break;

  case NBasicType:
    s = sdscatsds(s, n->t.basic.name);
    break;

  case NFunType: // TODO
    if (n->t.fun.params) {
      s = nodeRepr(n->t.fun.params, s, ctx, depth + 1);
    } else {
      s = sdscat(s, "()");
    }
    s = sdscat(s, "->");
    if (n->t.fun.result) {
      s = nodeRepr(n->t.fun.result, s, ctx, depth + 1);
    } else {
      s = sdscat(s, "()");
    }
    break;

  // uses u.op
  case NOp:
  case NAssign:
  case NPrefixOp:
  case NReturn:
    if (n->op.op != TNone) {
      s = sdscat(s, TokName(n->op.op));
      s = sdscatlen(s, " ", 1);
    }
    s = nodeRepr(n->op.left, s, ctx, depth + 1);
    if (n->op.right) {
      s = nodeRepr(n->op.right, s, ctx, depth + 1);
    }
    break;

  // uses u.array
  case NBlock:
  case NTuple:
  case NFile:
  {
    sdssetlen(s, sdslen(s)-1); // trim away trailing " " from s
    NodeListForEach(&n->array.a, n, {
      s = nodeRepr(n, s, ctx, depth + 1);
    });
    break;
  }

  // uses u.t.tuple
  case NTupleType: {
    s = sdscatlen(s, "(", 1);
    bool first = true;
    NodeListForEach(&n->t.tuple, n, {
      if (first) {
        first = false;
      } else {
        s = sdscatlen(s, " ", 1);
      }
      s = nodeRepr(n, s, ctx, depth + 1);
    });
    s = sdscatlen(s, ")", 1);
    break;
  }

  // uses u.field
  case NLet:
  case NVar:
  case NConst:
  case NField:
  {
    auto f = &n->field;
    if (f->name) {
      s = sdscatsds(s, f->name);
    } else {
      s = sdscatlen(s, "_", 1);
    }
    if (f->init) {
      s = nodeRepr(f->init, s, ctx, depth + 1);
    }
    break;
  }

  // uses u.fun
  case NFun: {
    auto f = &n->fun;

    if (f->name) {
      s = sdscatsds(s, f->name);
    } else {
      s = sdscatlen(s, "_", 1);
    }

    s = TStyleRed(s);
    s = sdscatprintf(s, " %p", n);
    s = TStyleNone(s);

    if (f->params) {
      s = nodeRepr(f->params, s, ctx, depth + 1);
    } else {
      s = reprEmpty(s, ctx);
    }

    if (f->body) {
      s = nodeRepr(f->body, s, ctx, depth + 1);
    }

    break;
  }

  // uses u.call
  case NCall: {
    // s = nodeRepr(n->call.receiver, s, ctx, depth + 1);
    auto recv = n->call.receiver;
    if (recv->fun.name) {
      s = sdscatsds(s, recv->fun.name);
    } else {
      s = sdscatlen(s, "_", 1);
    }
    s = TStyleRed(s);
    s = sdscatprintf(s, " %p", recv);
    s = TStyleNone(s);

    s = nodeRepr(n->call.args, s, ctx, depth + 1);
    break;
  }

  // uses u.cond
  case NIf:
    s = nodeRepr(n->cond.cond, s, ctx, depth + 1);
    s = nodeRepr(n->cond.thenb, s, ctx, depth + 1);
    if (n->cond.elseb) {
      s = nodeRepr(n->cond.elseb, s, ctx, depth + 1);
    }
    break;

  case _NodeKindMax: break;
  // Note: No default case, so that the compiler warns us about missing cases.

  }

  ctx->ind -= 2;
  PtrMapDel(&ctx->seen, (void*)n);

  if (!isType) {
    s = sdscatlen(s, ")", 1);
  }
  return s;
}


Str NodeRepr(const Node* n, Str s) {
  ReprCtx ctx = { 0 };
  ctx.maxdepth = 48;
  ctx.pretty = true;
  ctx.includeTypes = true;
  PtrMapInit(&ctx.seen, 32, NULL);
  s = nodeRepr(n, s, &ctx, /*depth*/ 1);
  PtrMapFree(&ctx.seen);
  return s;
}


ConstStr NodeReprShort(const Node* n) {
  // return a short string representation of a node, suitable for use in error messages.
  // Important: The returned string is invalidated on the next call to NodeReprShort,
  // so either copy it into an sds Str or make use of it right away.

  // Note: Do not include type information.
  // Instead, in use sites, call NodeReprShort individually for n->type when needed.

  // TODO: Rewrite all of this to use TmpData instead of sds

  ReprCtx ctx = { 0 };
  ctx.maxdepth = 1;
  ctx.pretty = false;
  ctx.includeTypes = false;
  PtrMapInit(&ctx.seen, 32, NULL);

  auto s = nodeRepr(n, sdsempty(), &ctx, /*depth*/ 1);

  PtrMapFree(&ctx.seen);

  // copy into tmpdata which is safe to return
  char* buf = TmpData(sdslen(s) + 1);
  memcpy(buf, s, sdslen(s) + 1);
  sdsfree(s);

  return buf;
}


// static void nodeArrayGrow(Memory mem, NodeArray* a, size_t addl) {
//   u32 reqcap = a->len + addl;
//   if (a->cap < reqcap) {
//     u32 cap = align2(reqcap, sizeof(Node) * 4);
//     // allocate new memory
//     void* m2 = memalloc(mem, sizeof(Node) * cap);
//     memcpy(m2, a->items, sizeof(Node) * a->cap);
//     a->items = (Node**)m2;
//     a->cap = cap;
//   }
// }


void NodeListAppend(Memory mem, NodeList* a, Node* n) {
  auto l = (NodeListLink*)memalloc(mem, sizeof(NodeListLink));
  l->node = n;
  if (a->tail == NULL) {
    a->head = l;
  } else {
    a->tail->next = l;
  }
  a->tail = l;
  a->len++;
}


Scope* ScopeNew(const Scope* parent, Memory mem) {
  auto s = (Scope*)memalloc(mem, sizeof(Scope));
  s->parent = parent;
  s->childcount = 0;
  SymMapInit(&s->bindings, 8, mem);
  return s;
}


static const Scope* globalScope = NULL;


void ScopeFree(Scope* s, Memory mem) {
  SymMapFree(&s->bindings);
  memfree(mem, s);
}


const Scope* GetGlobalScope() {
  if (globalScope == NULL) {
    auto s = ScopeNew(NULL, NULL);

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
  if (n == NULL) {
    dlog("lookup %s => (null)", s);
  } else {
    dlog("lookup %s => node of kind %s", s, NodeKindName(n->kind));
  }
  #endif
  return n;
}
