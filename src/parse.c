#include "wp.h"
#include "parseint.h"

// enable debug messages for pushScope() and popScope()
// #define DEBUG_SCOPE_PUSH_POP

// enable debug messages for defsym()
// #define DEBUG_DEFSYM


typedef enum Precedence {
  PREC_LOWEST,
  PREC_ASSIGN,
  PREC_COMMA,
  PREC_NULL_JOIN,
  PREC_LOGICAL_OR,
  PREC_LOGICAL_AND,
  PREC_BITWISE_OR,
  PREC_BITWISE_XOR,
  PREC_BITWISE_AND,
  PREC_EQUAL,
  PREC_COMPARE,
  PREC_SHIFT,
  PREC_ADD,
  PREC_MULTIPLY,
  PREC_UNARY_PREFIX,
  PREC_UNARY_POSTFIX,
  PREC_MEMBER,
} Precedence;


typedef struct Parselet Parselet;

typedef Node* (ParseletPrefixFun)(P* p);
typedef Node* (ParseletFun)      (P* p, const Parselet* e, Node* left);

typedef struct Parselet {
  ParseletPrefixFun* fprefix;
  ParseletFun*       f;
  Precedence         prec;
} Parselet;

#define next(p) SNext(&(p)->s)


inline static bool sdshasprefix(sds s, const char* prefix) {
  size_t prefixlen = strlen(prefix);
  return sdslen(s) >= prefixlen ? memcmp(s, prefix, prefixlen) == 0 : false;
}


// syntaxerrp reports a source-token related syntax error.
// It will point to the source location of the last-scanned token.
// If n is not NULL, use source location of n instead of current location.
//
static void syntaxerrp(P* p, SrcPos pos, const char* format, ...) {
  if (pos.src == null) {
    pos = SSrcPos(&p->s);
  }

  va_list ap;
  va_start(ap, format);
  auto msg = sdsempty();
  if (strlen(format) > 0) {
    msg = sdscatvprintf(msg, format, ap);
    assert(sdslen(msg) > 0); // format may contain %S which is not supported by sdscatvprintf
  }
  va_end(ap);

  const char* tokname;
  if (p->s.tok == TNil) {
    tokname = "end of input";
  } else if (p->s.tok == TSemi && p->s.inp > p->s.src->buf && *(p->s.inp - 1) == '\n') {
    tokname = "newline";
  } else {
    tokname = TokName(p->s.tok);
  }

  sds stmp = null;
  if (sdslen(msg) == 0) {
    stmp = msg;
    msg = sdsnewfmt("unexpected %s", tokname);
  } else if (sdshasprefix(msg, "expecting ")) {
    stmp = msg;
    msg = sdsnewfmt("unexpected %s, %S", tokname, msg);
  } else if (
    sdshasprefix(msg, "after ") ||
    sdshasprefix(msg, "in ") ||
    sdshasprefix(msg, "at ")
  ) {
    stmp = msg;
    msg = sdsnewfmt("unexpected %s %S", tokname, msg);
  }
  if (stmp) {
    sdsfree(stmp);
  }

  if (p->s.errh) {
    p->s.errh(p->s.src, pos, msg, p->s.userdata);
  }

  sdsfree(msg);
}


// syntaxerr = syntaxerrp(p, <srcpos of current token>, ...)
#define syntaxerr(p, format, ...) \
  syntaxerrp((p), NoSrcPos, format, ##__VA_ARGS__)


// toklistHas returns true if t is in list (list is expected to be 0-terminated)
static bool toklistHas(const Tok* list, Tok t) {
  Tok t2;
  while ((t2 = *list++)) {
    if (t2 == t) {
      return true;
    }
  }
  return false;
}

// advance consumes tokens until it finds a token of the followlist.
// It is used to recover from parse errors and is not speed critical.
//
static void advance(P* p, const Tok* followlist) {
  next(p); // guarantee progress
  if (followlist == null || *followlist == 0) {
    return;
  }
  if (p->fnest > 0) {
    // Stop at keywords that start a statement.
    // They are good synchronization points in case of syntax
    // errors and (usually) shouldn't be skipped over.
    while (!toklistHas(followlist, p->s.tok)) {
      switch (p->s.tok) {
        case TNil:
        case TBreak:
        case TConst:
        case TContinue:
        case TDefer:
        case TFallthrough:
        case TFor:
        case TGo:
        // case TGoto:
        case TIf:
        case TReturn:
        case TSelect:
        case TSwitch:
        case TType:
        case TVar:
          return;
        default:
          break;
      }
      // dlog("skip %s", TokName(p->s.tok));
      next(p);
    }
  } else {
    while (p->s.tok != TNil && !toklistHas(followlist, p->s.tok)) {
      // dlog("skip %s", TokName(p->s.tok));
      next(p);
    }
  }
  if (p->s.tok == TSemi) {
    next(p);
  }
}


// allocate a new ast node
inline static Node* PNewNode(P* p, NodeKind kind) {
  auto n = NewNode(&p->cc->mem, kind);
  n->pos.src = p->s.src;
  n->pos.offs = p->s.tokstart - p->s.src->buf;
  n->pos.span = p->s.tokend - p->s.tokstart;  assert(p->s.tokend >= p->s.tokstart);
  return n;
}

// precedence should match the calling parselet's own precedence
static Node* expr(P* p, int precedence);

// exprList = Expr ("," Expr)*
static Node* exprList(P* p, int precedence);

// exprOrList = Expr | exprList
static Node* exprOrList(P* p, int precedence);

// Fun = "fun" Ident Params? Type? Block?
static Node* pfun(P* p, bool nameOptional);

// pushScope adds a new scope to the stack. Returns the new scope.
static Scope* pushScope(P* p) {
  auto s = ScopeNew(p->scope);
  p->scope = s;
  #ifdef DEBUG_SCOPE_PUSH_POP
  dlog("push scope #%p", s);
  #endif
  return s;
}

// popScope removes & returns the topmost scope
static Scope* popScope(P* p) {
  auto s = p->scope;
  p->scope = (Scope*)s->parent; // note: discard const qual
  assert(p->scope != NULL);
  assert(p->scope != GetGlobalScope());

  #ifdef DEBUG_SCOPE_PUSH_POP
  dlog("pop scope #%p", s);
  if (s->bindings.len == 0 && s->childcount == 0) {
    dlog("  unused scope (free)");
  } else {
    dlog("  used scope (keep)");
  }
  #endif

  if (s->bindings.len == 0 && s->childcount == 0) {
    // the scope is unused and has no dependants; free it and return null
    ScopeFree(s);
    return NULL;
  }
  if (p->scope) {
    p->scope->childcount++;
  }
  return s;
}


static const Node* defsym(P* p, Sym s, Node* n) {
  const Node* existing = ScopeAssoc(p->scope, s, n);
  #ifdef DEBUG_DEFSYM
  if (existing) {
    dlog("defsym %s => %s (replacing %s)", s, NodeKindName(n->kind), NodeKindName(existing->kind));
  } else {
    dlog("defsym %s => %s", s, NodeKindName(n->kind));
  }
  #endif
  return existing;
}


// If the current token is t, advances scanner and returns true.
inline static bool got(P* p, Tok t) {
  if (p->s.tok == t) {
    next(p);
    return true;
  }
  return false;
}

// want reports a syntax error if p->s.tok != t.
// In any case, this function will advance the scanner by one token.
inline static void want(P* p, Tok t) {
  if (!got(p, t)) {
    syntaxerr(p, "expecting %s", TokName(t));
    next(p);
  }
}

static Node* bad(P* p) {
  return PNewNode(p, NBad);
}


// exprListTrailingComma = Expr ("," Expr)* ","?
static Node* exprListTrailingComma(P* p, int precedence, Tok stoptok) {
  auto list = PNewNode(p, NList);
  do {
    NodeListAppend(&p->cc->mem, &list->u.array.a, expr(p, precedence));
  } while (got(p, TComma) && p->s.tok != stoptok);
  return list;
}

// ============================================================================================
// ============================================================================================
// Parselets

//!PrefixParselet TIdent
static Node* PIdent(P* p) {
  auto n = PNewNode(p, NIdent);
  n->u.ref.name = p->s.name;
  next(p);
  return n;
}

static Node* pident(P* p) {
  if (p->s.tok != TIdent) {
    syntaxerr(p, "expecting identifier");
    next(p);
    return bad(p);
  }
  return PIdent(p);
}

//!Parselet (TEq ASSIGN)
static Node* PAssign(P* p, const Parselet* e, Node* left) {
  auto n = PNewNode(p, NAssign);
  n->u.op.op = p->s.tok;
  next(p);
  auto right = exprOrList(p, e->prec);
  n->u.op.left = left;
  n->u.op.right = right;

  // defsym
  if (left->kind == NList) {
    if (right->kind != NList) {
      syntaxerrp(p, left->pos, "assignment mismatch: %u targets but 1 value",
        left->u.array.a.len);
    } else {
      auto lnodes  = &left->u.array.a;
      auto rnodes = &right->u.array.a;
      if (lnodes->len != rnodes->len) {
        syntaxerrp(p, left->pos, "assignment mismatch: %u targets but %u values",
          lnodes->len, rnodes->len);
      } else {
        auto l = lnodes->head;
        auto r = rnodes->head;
        while (l) {
          if (l->node->kind == NIdent) {
            defsym(p, l->node->u.ref.name, r->node);
          } else {
            // e.g. foo.bar = 3
            dlog("TODO PAssign l->node->kind != NIdent");
          }
          l = l->next;
          r = r->next;
        }
      }
    }
  } else if (right->kind == NList) {
    syntaxerrp(p, left->pos, "assignment mismatch: 1 target but %u values",
      right->u.array.a.len);
  } else if (left->kind == NIdent) {
    defsym(p, left->u.ref.name, right);
  }

  return n;
}

// ptype = Type
//
static Node* ptype(P* p) {
  if (p->s.tok == TIdent) {
    return PIdent(p);
  } else if (p->s.tok == TFun) {
    return pfun(p, /*nameOptional*/true);
  }
  syntaxerr(p, "expecting type");
  return bad(p);
}


// helper for PVar to parse a multi-name definition
static Node* pVarMulti(P* p, NodeKind nkind, Node* ident0) {
  auto names = PNewNode(p, NList);
  names->pos = ident0->pos;

  NodeListAppend(&p->cc->mem, &names->u.array.a, ident0);
  do {
    NodeListAppend(&p->cc->mem, &names->u.array.a, pident(p));
  } while (got(p, TComma));

  Node* type = p->s.tok != TEq ? ptype(p) : NULL;
  bool hasValues = got(p, TEq);

  if (!hasValues && nkind == NConst) {
    // no values. e.g. "var x, y, z int"
    syntaxerr(p, "expecting =expression");
  }

  // parse values
  u32 valcount = 0;
  auto namecount = names->u.array.a.len;
  for (auto nl = names->u.array.a.head; nl; nl = nl->next) {
    auto n = nl->node;
    Node* value;
    if (hasValues) {
      value = expr(p, PREC_MEMBER);
      valcount++;
      if (nl->next && !got(p, ',')) {
        // not last item and got something that's not a comma
        hasValues = false;
        syntaxerr(p, "assignment mismatch: %u targets but %u values", namecount, valcount);
      }
    } else {
      value = PNewNode(p, NZeroInit);
      value->type = type;
    }
    auto namekind = n->kind;
    n->kind = nkind; // repurpose node as NVar|NConst node
    if (namekind == NIdent) {
      auto name = n->u.ref.name; // copy since u.ref.name and u.field.name might overlap in memory
      n->u.field.name = name;
      defsym(p, name, n);
    }
    n->u.field.init = value;
  }

  if (got(p, ',')) {
    auto extra = exprList(p, PREC_MEMBER);
    syntaxerrp(p, extra->pos, "assignment mismatch: %u targets but %u values",
      namecount, namecount + extra->u.array.a.len);
  }

  return names;
}


// VarDecl   = "var" identList (Type? "=" exprList | Type)
// ConstDecl = "const" identList Type? "=" exprList
//
// e.g. "var x, y u32 = 3, 45"
//
//!PrefixParselet TConst TVar
static Node* PVar(P* p) {
  auto nkind = p->s.tok == TConst ? NConst : NVar;
  next(p); // consume "var" or "const"

  auto name = pident(p);
  if (got(p, TComma)) {
    return pVarMulti(p, nkind, name);
  }

  Node* type = p->s.tok != TEq ? ptype(p) : NULL;

  Node* value = NULL;
  if (got(p, TEq)) {
    value = expr(p, PREC_MEMBER);
    if (type == NULL) {
      type = value->type;
    }
  } else {
    if (nkind == NConst) {
      syntaxerr(p, "expecting =expression");
    }
    value = PNewNode(p, NZeroInit);
    value->type = type;
  }
  auto n = PNewNode(p, nkind);
  n->pos = name->pos;
  n->type = type;
  n->u.field.init = value;
  if (name->kind == NIdent) {
    n->u.field.name = name->u.ref.name;
    defsym(p, name->u.ref.name, n);
  }
  return n;
}


//!PrefixParselet TComment
static Node* PComment(P* p) {
  auto n = PNewNode(p, NComment);
  n->u.str.ptr = p->s.tokstart;
  n->u.str.len = p->s.tokend - p->s.tokstart;
  next(p);
  return n;
}

// Group = "(" Expr ("," Expr)* ")"
// Groups are used to control precedence.
//!PrefixParselet TLParen
static Node* PGroup(P* p) {
  next(p); // consume "("
  auto n = expr(p, PREC_LOWEST);
  want(p, ')');
  return n;
}

//!Parselet (TLParen MEMBER)
static Node* PCall(P* p, const Parselet* e, Node* receiver) {
  auto n = PNewNode(p, NCall);
  next(p); // consume "("
  n->u.call.receiver = receiver;
  n->u.call.args = exprListTrailingComma(p, PREC_LOWEST, ')');
  want(p, ')');
  return n;
}

// Block = "{" Expr* "}"
//!PrefixParselet TLBrace
static Node* PBlock(P* p) {
  auto n = PNewNode(p, NBlock);
  next(p); // consume "{"

  pushScope(p);

  while (p->s.tok != TNil && p->s.tok != '}') {
    // nlistPush(n, exprOrList(p, PREC_LOWEST));
    NodeListAppend(&p->cc->mem, &n->u.array.a, exprOrList(p, PREC_LOWEST));
    if (!got(p, ';')) {
      break;
    }
  }
  if (!got(p, '}')) {
    syntaxerr(p, "expecting ; or }");
    next(p);
  }

  n->u.array.scope = popScope(p);

  return n;
}

// PrefixOp = ( "+" | "-" | "!" ) Expr
//!PrefixParselet TPlus TMinus TBang
static Node* PPrefixOp(P* p) {
  auto n = PNewNode(p, NPrefixOp);
  n->u.op.op = p->s.tok;
  next(p);
  n->u.op.left = expr(p, PREC_LOWEST);
  return n;
}

// InfixOp = Expr ( "+" | "-" | "*" | "/" ) Expr
//!Parselet (TPlus ADD) (TMinus ADD)
//          (TStar MULTIPLY) (TSlash MULTIPLY)
//          (TLt COMPARE) (TGt COMPARE)
//          (TEqEq EQUAL) (TNEq EQUAL)
static Node* PInfixOp(P* p, const Parselet* e, Node* left) {
  auto n = PNewNode(p, NOp);
  n->u.op.op = p->s.tok;
  n->u.op.left = left;
  next(p);
  n->u.op.right = expr(p, e->prec);
  return n;
}

// PostfixOp = Expr ( "++" | "--" )
//!Parselet (TPlusPlus UNARY_POSTFIX) (TMinusMinus UNARY_POSTFIX)
static Node* PPostfixOp(P* p, const Parselet* e, Node* operand) {
  auto n = PNewNode(p, NOp);
  n->u.op.op = p->s.tok;
  n->u.op.left = operand;
  next(p); // consume "+"
  return n;
}

// IntLit = [0-9]+
//!PrefixParselet TIntLit
static Node* PIntLit(P* p) {
  auto n = PNewNode(p, NInt);
  size_t len = p->s.tokend - p->s.tokstart;
  if (!parseint64((const char*)p->s.tokstart, len, /*base*/10, &n->u.integer)) {
    n->u.integer = 0;
    syntaxerrp(p, n->pos, "invalid integer literal");
  }
  next(p);
  n->type = (Node*)Type_int;
  return n;
}

// If = "if" Expr Expr
//!PrefixParselet TIf
static Node* PIf(P* p) {
  auto n = PNewNode(p, NIf);
  next(p);
  n->u.cond.cond = expr(p, PREC_LOWEST);
  n->u.cond.thenb = expr(p, PREC_LOWEST);
  if (p->s.tok == TElse) {
    next(p);
    n->u.cond.elseb = expr(p, PREC_LOWEST);
  }
  return n;
}

// Return = "return" Expr?
//!PrefixParselet TReturn
static Node* PReturn(P* p) {
  auto n = PNewNode(p, NReturn);
  next(p);
  if (p->s.tok != ';' && p->s.tok != '}') {
    n->u.op.left = exprOrList(p, PREC_LOWEST);
  }
  return n;
}


// params = "(" param ("," param)* ","? ")"
// param  = Ident Type? | Type
//
static Node* params(P* p) {
  // examples:
  //
  // (T)
  // (x T)
  // (x, y, z T)
  // (... T)
  // (x  ... T)
  // (x, y, z  ... T)
  // (T1, T2, T3)
  // (T1, T2, ... T3)
  //
  want(p, '(');
  auto n = PNewNode(p, NList);
  bool hasTypedParam = false; // true when at least one param has type; e.g. "x T"
  NodeList typeq;

  while (p->s.tok != TRParen && p->s.tok != TNil) {
    auto field = PNewNode(p, NField);
    if (p->s.tok == TIdent) {
      field->u.field.name = p->s.name;
      next(p);
      // TODO: check if "<" follows. If so, this is a type.
      if (p->s.tok != TRParen && p->s.tok != TComma && p->s.tok != TSemi) {
        field->type = expr(p, PREC_LOWEST);
        hasTypedParam = true;
        // spread type to predecessors
        if (typeq.len > 0) {
          NodeListForEach(&typeq, field2, {
            field2->type = field->type;
          });
          NodeListClear(&typeq);
        }
      } else {
        NodeListAppend(&p->cc->mem, &typeq, field);
      }
    } else {
      // definitely just type, e.g. "fun(int)int"
      field->type = expr(p, PREC_LOWEST);
    }
    NodeListAppend(&p->cc->mem, &n->u.array.a, field);
    if (!got(p, TComma)) {
      if (p->s.tok != TRParen) {
        syntaxerr(p, "expecting comma or )");
        next(p);
      }
      break;
    }
  }

  if (hasTypedParam) {
    // name-and-type form; e.g. "(x, y T, z Y)"
    if (typeq.len > 0) {
      // at least one param has type, but the last one does not.
      // e.g. "(x, y int, z)"
      syntaxerr(p, "expecting type");
    }
    NodeListForEach(&n->u.array.a, field,
      defsym(p, field->u.field.name, field));
  } else {
    // type-only form; e.g. "(T, T, Y)"
    // make ident of each field->u.field.name where field->type == NULL
    NodeListForEach(&n->u.array.a, field, {
      if (!field->type) {
        auto t = PNewNode(p, NIdent);
        t->u.ref.name = field->u.field.name;
        field->type = t;
        field->u.field.name = sym__;
      }
    });
  }

  want(p, ')');
  return n;
}


// Fun     = FunDef | FunExpr
// FunDef  = "fun" Ident? params? Type? Block?
// FunExpr = "fun" Ident? params? Type? "->" Expr
//
// e.g.
//   fun foo (x, y int) int
//   fun foo (x, y int) int { x * y }
//   fun foo { 5 }
//   fun foo -> 5
//   fun (x, y int) int { x * y }
//   fun { 5 }
//   fun -> 5
//
static Node* pfun(P* p, bool nameOptional) {
  auto n = PNewNode(p, NFun);
  next(p);
  // name
  if (p->s.tok == TIdent) {
    auto name = p->s.name;
    n->u.fun.name = name;
    defsym(p, name, n);
    next(p);
  } /*else if (!nameOptional) {
    syntaxerr(p, "expecting name");
    next(p);
  }*/
  // parameters
  pushScope(p);
  if (p->s.tok == '(') {
    n->u.fun.params = params(p);
  }
  // result type(s)
  if (p->s.tok != '{' && p->s.tok != ';' && p->s.tok != TRArr) {
    n->type = exprOrList(p, PREC_LOWEST);
  }
  // body
  p->fnest++;
  if (p->s.tok == '{') {
    n->u.fun.body = PBlock(p);
  } else if (got(p, TRArr)) {
    n->u.fun.body = exprOrList(p, PREC_LOWEST);
  }
  p->fnest--;
  n->u.fun.scope = popScope(p);
  return n;
}

// See pfun for syntax
//!PrefixParselet TFun
static Node* PFun(P* p) {
  return pfun(p, /* nameOptional */ false);
}


// end of parselets
// ============================================================================================
// ============================================================================================


//PARSELET_MAP_BEGIN
// automatically generated by misc/gen_parselet_map.py; do not edit
static const Parselet parselets[TMax] = {
  [TIdent] = {PIdent, NULL, PREC_MEMBER},
  [TConst] = {PVar, NULL, PREC_MEMBER},
  [TVar] = {PVar, NULL, PREC_MEMBER},
  [TComment] = {PComment, NULL, PREC_MEMBER},
  [TLParen] = {PGroup, PCall, PREC_MEMBER},
  [TLBrace] = {PBlock, NULL, PREC_MEMBER},
  [TPlus] = {PPrefixOp, PInfixOp, PREC_ADD},
  [TMinus] = {PPrefixOp, PInfixOp, PREC_ADD},
  [TBang] = {PPrefixOp, NULL, PREC_MEMBER},
  [TIntLit] = {PIntLit, NULL, PREC_MEMBER},
  [TIf] = {PIf, NULL, PREC_MEMBER},
  [TReturn] = {PReturn, NULL, PREC_MEMBER},
  [TFun] = {PFun, NULL, PREC_MEMBER},
  [TEq] = {NULL, PAssign, PREC_ASSIGN},
  [TStar] = {NULL, PInfixOp, PREC_MULTIPLY},
  [TSlash] = {NULL, PInfixOp, PREC_MULTIPLY},
  [TLt] = {NULL, PInfixOp, PREC_COMPARE},
  [TGt] = {NULL, PInfixOp, PREC_COMPARE},
  [TEqEq] = {NULL, PInfixOp, PREC_EQUAL},
  [TNEq] = {NULL, PInfixOp, PREC_EQUAL},
  [TPlusPlus] = {NULL, PPostfixOp, PREC_UNARY_POSTFIX},
  [TMinusMinus] = {NULL, PPostfixOp, PREC_UNARY_POSTFIX},
};
//PARSELET_MAP_END


inline static Node* prefixExpr(P* p) {
  // find prefix parselet
  assert((u32)p->s.tok < (u32)TMax);
  auto parselet = &parselets[p->s.tok];
  if (!parselet->fprefix) {
    syntaxerr(p, "expecting expression");
    auto n = bad(p);
    Tok followlist[] = { ')', '}', ']', ';', 0 };
    advance(p, followlist);
    return n;
  }
  return parselet->fprefix(p);
}


inline static Node* infixExpr(P* p, int precedence, Node* left) {
  // wrap parselets
  while (p->s.tok != TNil) {
    auto parselet = &parselets[p->s.tok];
    // if (parselet->f) {
    //   dlog("found infix parselet for %s; parselet->prec=%d < precedence=%d = %s",
    //     TokName(p->s.tok), parselet->prec, precedence, parselet->prec < precedence ? "Y" : "N");
    // }
    if (parselet->prec < precedence || !parselet->f) {
      break;
    }
    assert(parselet);
    left = parselet->f(p, parselet, left);
  }
  return left;
}


static Node* expr(P* p, int precedence) {
  // Note: precedence should match the calling parselet's own precedence
  auto left = prefixExpr(p);
  return infixExpr(p, precedence, left);
}


// exprOrList = Expr | exprList
static Node* exprOrList(P* p, int precedence) {
  // // skip semicolons
  // while (got(p, TSemi)) {
  //   next(p);
  // }

  // parse prefix
  auto left = prefixExpr(p);

  // if a comma appears after a primary expression, read more primary expressions and group
  // them into an List.
  if (got(p, TComma)) {
    auto g = PNewNode(p, NList);
    NodeListAppend(&p->cc->mem, &g->u.array.a, left);
    do {
      NodeListAppend(&p->cc->mem, &g->u.array.a, prefixExpr(p));
    } while (got(p, TComma));
    left = g;
  }

  return infixExpr(p, precedence, left);
}


// exprList = Expr ("," Expr)*
static Node* exprList(P* p, int precedence) {
  auto list = PNewNode(p, NList);
  do {
    NodeListAppend(&p->cc->mem, &list->u.array.a, expr(p, precedence));
  } while (got(p, TComma));
  return list;
}


Node* Parse(P* p, CCtx* cc, ScanFlags sflags, Scope* pkgscope) {
  // initialize scanner
  SInit(&p->s, &cc->src, sflags, cc->errh, cc->userdata);
  p->fnest = 0;
  p->scope = pkgscope;
  p->cc = cc;
  next(p); // read first token

  // TODO: ParseFlags, where one option is PARSE_IMPORTS to parse only imports and then stop.

  auto file = PNewNode(p, NFile);
  pushScope(p);

  while (p->s.tok != TNil) {
    Node* n = exprOrList(p, PREC_LOWEST);
    NodeListAppend(&p->cc->mem, &file->u.array.a, n);

    // // print associated comments
    // auto c = p->s.comments;
    // while (c) { printf("#%.*s\n", (int)c->len, c->ptr); c = c->next; }
    // p->s.comments = p->s.comments_tail = NULL;
    // // TODO: Add "comments" to Node struct and if caller requests inclusion of comments,
    // // assign these comments to the node. This should be done in PNewNode and not here.

    // // print AST representation
    // auto s = NodeRepr(n, sdsempty());
    // s = sdscatlen(s, "\n", 1);
    // fwrite(s, sdslen(s), 1, stdout);
    // sdsfree(s);

    // check that we either got a semicolon or EOF
    if (p->s.tok != TNil && !got(p, TSemi)) {
      syntaxerr(p, "after top level declaration");
      Tok followlist[] = { TType, TFun, TConst, TVar, TSemi, 0 };
      advance(p, followlist);
    }
  }

  file->u.array.scope = popScope(p);
  return file;
}



// //!Parselet (TComma COMMA)
// static Node* PList(P* p, const Parselet* e, Node* left) {
//   Node* n = left;
//   if (left->kind != NList) {
//     n = PNewNode(p, NList);
//     nlistPush(n, left);
//   }
//   next(p); // consume ','
//   auto right = expr(p, e->prec);
//   if (right->kind == NList) {
//     // steal members from exprlist (move list)
//     n->u.list.tail->link_next = right->u.list.head;
//     n->u.list.tail = right->u.list.head;
//     right->u.list.head = null;
//     right->u.list.tail = null;
//     NodeFree(right);
//   } else {
//     nlistPush(n, right);
//   }
//   return n;
// }
