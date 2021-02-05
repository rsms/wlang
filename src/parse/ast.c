#include "ast.h"
#include "../common/ptrmap.h"
#include "../common/tstyle.h"

// #define DEBUG_LOOKUP


// Lookup table N<kind> => name
static const char* const NodeKindNameTable[] = {
  #define I_ENUM(name, _cls) #name,
  DEF_NODE_KINDS(I_ENUM)
  #undef  I_ENUM
};

// Lookup table N<kind> => NClass<class>
const NodeClass NodeClassTable[_NodeKindMax] = {
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


const Node* NodeEffectiveType(const Node* n) {
  auto t = n->type;
  if (t == NULL) {
    t = Type_nil;
  } else if (NodeIsUntyped(n)) {
    t = IdealType(NodeIdealCType(n));
  }
  return t;
}


Node* IdealType(CType ct) {
  switch (ct) {
  case CType_int:   return Type_int;
  case CType_float: return Type_float64;
  case CType_str:   return Type_str;
  case CType_bool:  return Type_bool;
  case CType_nil:   return Type_nil;

  case CType_rune:
  case CType_INVALID: break;
  }
  dlog("err: unexpected CType %d", ct);
  assert(0 && "unexpected CType");
  return NULL;
}


// NodeIdealCType returns a type for an arbitrary "ideal" (untyped constant) expression like "3".
CType NodeIdealCType(const Node* n) {
  if (n == NULL || !NodeIsUntyped(n)) {
    return CType_INVALID;
  }
  dlog("NodeIdealCType n->kind = %s", NodeKindName(n->kind));

  switch (n->kind) {
  default:
    return CType_nil;

  case NIntLit:
  case NFloatLit:
    // Note: NBoolLit is always typed
    return n->val.ct;

  case NPrefixOp:
  case NPostfixOp:
    return NodeIdealCType(n->op.left);

  case NIdent:
    return NodeIdealCType(n->ref.target);

  case NBinOp:
    switch (n->op.op) {
      case TEq:       // "=="
      case TNEq:      // "!="
      case TLt:       // "<"
      case TLEq:      // "<="
      case TGt:       // ">"
      case TGEq:      // ">="
      case TAndAnd:   // "&&
      case TPipePipe: // "||
        return CType_bool;

      case TShl:
      case TShr:
        // shifts are always of left (receiver) type
        return NodeIdealCType(n->op.left);

      default: {
        auto L = NodeIdealCType(n->op.left);
        auto R = NodeIdealCType(n->op.right);
        return max(L, R); // pick the dominant type
      }
    }

  }
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


Str NValFmt(Str s, const NVal* v) {
  switch (v->ct) {

  case CType_int:
    if (v->i > 0x7fffffffffffffff) {
      return sdscatprintf(s, "%llu", v->i);
    } else {
      return sdscatprintf(s, "%lld", (i64)v->i);
    }

  case CType_rune:
  case CType_float:
  case CType_str:
    dlog("TODO NValFmt");
    break;

  case CType_bool:
    return sdscat(s, v->i == 0 ? "false" : "true");

  case CType_nil:
    sdscat(s, "nil");

  case CType_INVALID:
    assert(0 && "unexpected CType");
    break;
  }
  return sdscat(s, "?");
}


const char* NValStr(const NVal* v) {
  auto s = sdsempty();
  s = NValFmt(s, v);
  if (s == NULL) {
    return "";
  }
  return memgcsds(s);
}


static Str nodeRepr(const Node* n, Str s, ReprCtx* ctx, int depth) {
  if (n == NULL) {
    return sdscatlen(s, "(null)", 6);
  }

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
    s = TStyleGrey(s);
    s = sdscat(s, "...");
    s = TStyleNoColor(s);
    return s;
  }

  // cycle guard
  if (PtrMapSet(&ctx->seen, (void*)n, (void*)1) != NULL) {
    PtrMapDel(&ctx->seen, (void*)n);
    s = sdscatfmt(s, " [cyclic %s]", NodeKindNameTable[n->kind]);
    return s;
  }

  auto isType = NodeKindIsType(n->kind);
  if (!isType) {
    s = indent(s, ctx);

    if (n->kind != NFile && ctx->includeTypes) {
      s = TStyleBlue(s);
      if (n->type) {
        s = nodeRepr(n->type, s, ctx, depth + 1);
        s = TStyleBlue(s); // in case nodeRepr changed color
        s = sdscatlen(s, ":", 1);
      } else {
        s = sdscatlen(s, "?:", 2);
      }
      s = TStyleNoColor(s);
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
  case NIntLit:
    s = sdscatfmt(s, "%U", n->val.i);
    break;
  case NBoolLit:
    if (n->val.i == 0) {
      s = sdscat(s, "false");
    } else {
      s = sdscat(s, "true");
    }
    break;

  // uses u.real
  case NFloatLit:
    s = sdscatprintf(s, "%f", n->val.f);
    break;

  // uses u.str
  case NComment:
    s = sdscatrepr(s, (const char*)n->str.ptr, n->str.len);
    break;

  // uses u.ref
  case NIdent:
    s = TStyleRed(s);
    assert(n->ref.name != NULL);
    s = sdscatsds(s, n->ref.name);
    s = TStyleNoColor(s);
    if (n->ref.target) {
      s = sdscatfmt(s, " @%s", NodeKindNameTable[n->ref.target->kind]);
      // s = nodeRepr(n->ref.target, s, ctx, depth + 1);
    }
    break;

  // uses u.op
  case NBinOp:
  case NPostfixOp:
  case NPrefixOp:
  case NAssign:
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
    // sdssetlen(s, sdslen(s)-1); // trim away trailing " " from s
    NodeListForEach(&n->array.a, n, {
      s = nodeRepr(n, s, ctx, depth + 1);
      // TODO: detect if line breaks were added.
      // I.e. (Tuple int int) is currently printed as "(Tuple intint)" (missing space).
    });
    break;
  }

  // uses u.field
  case NLet:
  case NArg:
  case NField:
  {
    if (n->kind == NArg) {
      s = sdscatfmt(s, "#%u ", n->field.index);
    }
    if (n->field.name) {
      s = sdscatsds(s, n->field.name);
    } else {
      s = sdscatlen(s, "_", 1);
    }
    if (n->field.init != NULL) {
      s = nodeRepr(n->field.init, s, ctx, depth + 1);
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
    s = TStyleNoColor(s);

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
  case NTypeCast: // TODO
  case NCall: {
    auto recv = n->call.receiver;

    const Node* funTarget = (
      recv->kind == NFun ? recv :
      (recv->kind == NIdent && recv->ref.target != NULL && recv->ref.target->kind == NFun) ?
        recv->ref.target :
      NULL
    );

    if (funTarget != NULL) {
      // print receiver function when we know it
      if (funTarget->fun.name) {
        s = sdscatsds(s, funTarget->fun.name);
      } else {
        s = sdscatlen(s, "_", 1);
      }
      s = TStyleRed(s);
      s = sdscatprintf(s, " %p", funTarget);
      s = TStyleNoColor(s);
    } else if (recv->kind == NIdent && recv->ref.target == NULL) {
      // when the receiver is an ident without a resolved target, print its name
      s = sdscatsds(s, recv->ref.name);
    } else {
      s = nodeRepr(recv, s, ctx, depth + 1);
    }

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

  case NBasicType:
    s = TStyleBlue(s);
    if (n == Type_ideal) {
      s = sdscatlen(s, "*", 1);
    } else {
      s = sdscatsds(s, n->t.basic.name);
    }
    s = TStyleNoColor(s);
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
  PtrMapDealloc(&ctx.seen);
  return s;
}


static sds _sdscatNodeList(sds s, const NodeList* nodeList) {
  bool isFirst = true;
  NodeListForEach(nodeList, n, {
    if (isFirst) {
      isFirst = false;
    } else {
      s = sdscatc(s, ' ');
    }
    s = sdscatnode(s, n);
  });
  return s;
}


// appends a short representation of an AST node to s, suitable for use in error messages.
sds sdscatnode(sds s, const Node* n) {
  // Note: Do not include type information.
  // Instead, in use sites, call fmtnode individually for n->type when needed.

  if (n == NULL) {
    return sdscat(s, "nil");
  }

  switch (n->kind) {

  // uses no extra data
  case NNil: // nil
    s = sdscat(s, "nil");
    break;

  case NZeroInit: // init
    s = sdscat(s, "init");
    break;

  case NBoolLit: // true | false
    if (n->val.i == 0) {
      s = sdscat(s, "false");
    } else {
      s = sdscat(s, "true");
    }
    break;

  case NIntLit: // 123
    s = sdscatfmt(s, "%U", n->val.i);
    break;

  case NFloatLit: // 12.3
    s = sdscatprintf(s, "%f", n->val.f);
    break;

  case NComment: // #"comment"
    s = sdscat(s, "#\"");
    s = sdscatrepr(s, (const char*)n->str.ptr, n->str.len);
    s = sdscatc(s, '"');
    break;

  case NIdent: // foo
    s = sdscatsds(s, n->ref.name);
    break;

  case NBinOp: // foo+bar
    s = sdscatnode(s, n->op.left);
    s = sdscat(s, TokName(n->op.op));
    s = sdscatnode(s, n->op.right);
    break;

  case NPostfixOp: // foo++
    s = sdscatnode(s, n->op.left);
    s = sdscat(s, TokName(n->op.op));
    break;

  case NPrefixOp: // -foo
    s = sdscat(s, TokName(n->op.op));
    s = sdscatnode(s, n->op.left); // note: prefix op uses left, not right.
    break;

  case NAssign: // thing=
    s = sdscatnode(s, n->op.left);
    s = sdscatc(s, '=');
    break;

  case NReturn: // return thing
    s = sdscat(s, "return ");
    s = sdscatnode(s, n->op.left);
    break;

  case NBlock: // {int}
    s = sdscatc(s, '{');
    s = sdscatnode(s, n->type);
    s = sdscatc(s, '}');
    break;

  case NTuple: { // (one two 3)
    s = sdscatc(s, '(');
    s = _sdscatNodeList(s, &n->array.a);
    s = sdscatc(s, ')');
    break;
  }

  case NFile: // file
    s = sdscat(s, "file");
    break;

  case NLet: // let
    // s = sdscatfmt(s, "%S=", n->field.name);
    s = sdscat(s, "let");
    break;

  case NArg: // foo
    s = sdscatsds(s, n->field.name);
    break;

  case NFun: // fun foo
    if (n->fun.name == NULL) {
      s = sdscat(s, "fun _");
    } else {
      s = sdscatfmt(s, "fun %S", n->fun.name);
    }
    break;

  case NTypeCast: // typecast<int16>
    s = sdscat(s, "typecast<");
    s = sdscatnode(s, n->call.receiver);
    s = sdscatc(s, '>');
    break;

  case NCall: // call foo
    s = sdscat(s, "call ");
    s = sdscatnode(s, n->call.receiver);
    break;

  case NIf: // if
    s = sdscat(s, "if");
    break;

  case NBasicType: // int
    if (n == Type_ideal) {
      s = sdscat(s, "ideal");
    } else {
      s = sdscatsds(s, n->t.basic.name);
    }
    break;

  case NFunType: // (int int)->bool
    if (n->t.fun.params == NULL) {
      s = sdscat(s, "()");
    } else {
      s = sdscatnode(s, n->t.fun.params);
    }
    s = sdscat(s, "->");
    s = sdscatnode(s, n->t.fun.params); // ok if NULL
    break;

  case NTupleType: // (int bool Foo)
    s = sdscatc(s, '(');
    s = _sdscatNodeList(s, &n->t.tuple);
    s = sdscatc(s, ')');
    break;

  // The remaining types are not expected to appear. Use their kind if they do.
  case NBad:
  case NNone:
  case NField: // field is not yet implemented by parser
    s = sdscat(s, NodeKindNameTable[n->kind]);
    break;

  case _NodeKindMax:
    break;
  }

  return s;
}


ConstStr fmtnode(const Node* n) {
  auto s = sdscatnode(sdsnewcap(16), n);
  return memgcsds(s); // GC
}


ConstStr fmtast(const Node* n) {
  auto s = NodeRepr(n, sdsnewcap(32));
  return memgcsds(s); // GC
}


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
  SymMapDealloc(&s->bindings);
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
  return SymMapSet(&s->bindings, key, (Node*)value);
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
