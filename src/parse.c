#include "wp.h"
#include "parseint.h"
#include "array.h"

typedef enum Precedence { // taken from skew
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
  auto msg = sdscatvprintf(sdsempty(), format, ap);
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

  auto s = SrcPosMsg(sdsempty(), pos, msg);
  sdsfree(msg);

  fwrite(s, sdslen(s), 1, stderr);
  if (s != msg) {
    sdsfree(s);
  }
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
  auto n = (Node*)calloc(1, sizeof(Node));
  n->kind = kind;
  n->pos.src = p->s.src;
  n->pos.offs = p->s.tokstart - p->s.src->buf;
  n->pos.span = p->s.tokend - p->s.tokstart;  assert(p->s.tokend >= p->s.tokstart);
  return n;
}

inline static void freeNode(Node* n) {
  // just leak for now.
  // TODO: freelist
}

// shallow copy
inline static Node* cloneNode(Node* n) {
  auto n2 = (Node*)malloc(sizeof(Node));
  *n2 = *n;
  return n2;
}

// operations on node u.list (singly-linked list)
inline static void nlistPush(Node* list, Node* elem) {
  if (!list->u.list.head) {
    list->u.list.head = elem;
  } else {
    list->u.list.tail->link_next = elem;
  }
  list->u.list.tail = elem;
}

static u32 nlistCount(Node* n) {
  auto n2 = n->u.list.head;
  u32 count = 0;
  while (n2) {
    count++;
    n2 = n2->link_next;
  }
  return count;
}

// precedence should match the calling parselet's own precedence
static Node* expr(P* p, int precedence);

// exprList = Expr ("," Expr)*
static Node* exprList(P* p, int precedence);

// exprOrList = Expr | exprList
static Node* exprOrList(P* p, int precedence);

// Fun = "fun" Ident Params? Type? Block?
static Node* pfun(P* p);

// parseIdent etc
// generate node-specific parse functions for expression nodes (e.g. parseIdent)
#define I_ENUM(name) \
static Node* parse##name(P* p, int precedence) {                           \
  auto n = expr(p, precedence);                                      \
  if (n->kind != N##name) {                                                \
    syntaxerr((p), "expecting %s", NodeKindName(N##name));                 \
  }                                                                        \
  return n;                                                                \
}
DEF_NODE_KINDS_EXPR(I_ENUM)
#undef I_ENUM

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
    nlistPush(list, expr(p, precedence));
  } while (got(p, TComma) && p->s.tok != stoptok);
  return list;
}

// ============================================================================================
// ============================================================================================
// Parselets

//!PrefixParselet TIdent
static Node* PIdent(P* p) {
  auto n = PNewNode(p, NIdent);
  n->u.name = p->s.name;
  next(p);
  return n;
}

static Node* ident(P* p) {
  want(p, TIdent);
  return PIdent(p);
}

//!Parselet (TEq ASSIGN)
static Node* PAssign(P* p, const Parselet* e, Node* left) {
  auto n = PNewNode(p, NAssign);
  n->u.op.op = p->s.tok;
  n->u.op.left = left;
  next(p);
  n->u.op.right = exprOrList(p, e->prec);

  // TODO: maybe move this to bind/resolve pass?
  // check that count(left) == count(right)
  auto right  = n->u.op.right;
  auto nleft  = left->kind  != NList ? 1 : nlistCount(left);
  auto nright = right->kind != NList ? 1 : nlistCount(right);
  if (nleft != nright) {
    syntaxerrp(p, n->pos, "assignment mismatch: %u targets but %u values", nleft, nright);
  }

  return n;
}

// maybeType = Type?
//
static Node* maybeType(P* p) {
  if (p->s.tok == TIdent) {
    return parseIdent(p, PREC_MEMBER);
  } else if (p->s.tok == TFun) {
    return pfun(p);
  }
  return null;
}


// VarDecl   = "var" identList (Type? "=" exprList | Type)
// ConstDecl = "const" identList Type? "=" exprList
//
// e.g. "var x, y u32 = 3, 45"
//
//!PrefixParselet TConst TVar
static Node* PVar(P* p) {
  auto n = PNewNode(p, p->s.tok == TConst ? NConst : NVar);
  next(p);
  n->u.op.left = exprList(p, PREC_MEMBER);
  n->type = maybeType(p);
  if (got(p, TEq)) {
    n->u.op.right = exprList(p, PREC_MEMBER);
  } else if (!n->type) {
    syntaxerr(p, "expecting type");
    next(p);
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
  while (p->s.tok != TNil && p->s.tok != '}') {
    nlistPush(n, exprOrList(p, PREC_LOWEST));
    if (!got(p, ';')) {
      break;
    }
  }
  if (!got(p, '}')) {
    syntaxerr(p, "expecting ; or }");
    next(p);
  }
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
  bool hasTypedParam = false; // true if at least one param has type; e.g. "x T"
  Array typeq = Array_STACK_INIT(16); // fields awaiting type spread

  while (p->s.tok != TRParen && p->s.tok != TNil) {
    auto field = PNewNode(p, NField);
    if (p->s.tok == TIdent) {
      field->u.field.name = p->s.name;
      next(p);
      // TODO: check if "<" follows -- if so, this is a type.
      if (p->s.tok != TRParen && p->s.tok != TComma && p->s.tok != TSemi) {
        field->type = expr(p, PREC_LOWEST);
        hasTypedParam = true;
        // spread type to predecessors
        if (typeq.len > 0) {
          for (u32 i = 0; i < typeq.len; i++) {
            auto field2 = (Node*)typeq.v[i];
            field2->type = field->type;
          }
          typeq.len = 0;
        }
      } else {
        ArrayPush(&typeq, field);
      }
    } else {
      // definitely just type, e.g. "fun(int)int"
      field->type = expr(p, PREC_LOWEST);
    }
    nlistPush(n, field);
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
  } else {
    // type-only form; e.g. "(T, T, Y)"
    // make ident of each field->u.field.name where field->type == NULL
    auto field = n->u.list.head;
    while (field) {
      if (!field->type) {
        field->type = PNewNode(p, NIdent);
        field->type->u.name = field->u.field.name;
        field->u.field.name = sym__;
      }
      field = field->link_next;
    }
  }

  want(p, ')');
  ArrayFree(&typeq);
  return n;
}

// Fun = "fun" Ident? params? Type? Block?
//
// e.g.
//   fun foo (x, y int) int { x * y }
//   fun foo { 5 }
//   fun foo -> int 5
//
static Node* pfun(P* p) {
  auto n = PNewNode(p, NFun);
  next(p);
  if (p->s.tok == TIdent) {
    n->u.fun.name = p->s.name;
    next(p);
  }
  if (p->s.tok == '(') {
    n->u.fun.params = params(p);
  }
  if (p->s.tok != '{' && p->s.tok != ';') {
    n->type = exprOrList(p, PREC_LOWEST);
  }
  if (p->s.tok == '{') {
    n->u.fun.body = PBlock(p);
  }
  return n;
}

// Fun = "fun" Ident Params? Type? Block?
//
// e.g.
//   fun foo (x, y int) int { x * y }
//   fun foo { 5 }
//   fun foo -> int 5
//
//!PrefixParselet TFun
static Node* PFun(P* p) {
  auto n = pfun(p);
  if (n->u.fun.name == null) {
    syntaxerrp(p, n->pos, "expecting name");
  }
  return n;
}


// end of parselets
// ============================================================================================
// ============================================================================================


void PInit(P* p, Source* src) {
  // initialize scanner
  SInit(&p->s, src, SCAN_COMMENTS);
  p->fnest = 0;
  next(p); // read first token
}

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
    nlistPush(g, left);
    do {
      nlistPush(g, prefixExpr(p));
    } while (got(p, TComma));
    left = g;
  }

  return infixExpr(p, precedence, left);
}


// exprList = Expr ("," Expr)*
static Node* exprList(P* p, int precedence) {
  auto list = PNewNode(p, NList);
  do {
    nlistPush(list, expr(p, precedence));
  } while (got(p, TComma));
  return list;
}


void PParseFile(P* p) {
  while (p->s.tok != TNil) {
    Node* n = exprOrList(p, PREC_LOWEST);

    // // print associated comments
    // auto c = p->s.comments;
    // while (c) { printf("#%.*s\n", (int)c->len, c->ptr); c = c->next; }
    // p->s.comments = p->s.comments_tail = NULL;

    // print AST representation
    auto s = NodeRepr(n, sdsempty());
    s = sdscatlen(s, "\n", 1);
    fwrite(s, sdslen(s), 1, stdout);
    sdsfree(s);

    // check that we either got a semicolon or EOF
    if (p->s.tok != TNil && !got(p, TSemi)) {
      syntaxerr(p, "after top level declaration");
      Tok followlist[] = { TType, TFun, TConst, TVar, TSemi, 0 };
      advance(p, followlist);
    }
  }
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
//     freeNode(right);
//   } else {
//     nlistPush(n, right);
//   }
//   return n;
// }
