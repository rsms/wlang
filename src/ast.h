#pragma once
#include "array.h"
#include "memory.h"

typedef enum {
  NodeClassInvalid = 0,
  NodeClassConst,  // literals like 123, true, nil.
  NodeClassExpr,
  NodeClassType,
} NodeClass;

// Primary node kinds which are either expressions or start of expressions
#define DEF_NODE_KINDS(_) \
  /* N<kind>     NClass<class> */ \
  _(None,        Invalid) \
  _(Bad,         Invalid) /* substitute "filler node" for invalid syntax */ \
  _(BoolLit,     Const) /* boolean literal */ \
  _(IntLit,      Const) /* integer literal */ \
  _(FloatLit,    Const) /* floating-point literal */ \
  _(Nil,         Const) /* the nil atom */ \
  _(Comment,     Expr) \
  _(Assign,      Expr) \
  _(Arg,         Expr) \
  _(Block,       Expr) \
  _(Call,        Expr) \
  _(Field,       Expr) \
  _(File,        Expr) \
  _(Fun,         Expr) \
  _(Ident,       Expr) \
  _(If,          Expr) \
  _(Let,         Expr) \
  _(BinOp,       Expr) \
  _(PostfixOp,   Expr) \
  _(PrefixOp,    Expr) \
  _(Return,      Expr) \
  _(Tuple,       Expr) \
  _(TypeCast,    Expr) \
  _(ZeroInit,    Expr) \
  _(BasicType,   Type) /* Basic type, e.g. int, bool */ \
  _(TupleType,   Type) /* Tuple type, e.g. (float,bool,int) */ \
  _(FunType,     Type) /* Function type, e.g. (int,int)->(float,bool) */ \
/*END DEF_NODE_KINDS*/

typedef enum {
  #define I_ENUM(name, _cls) N##name,
  DEF_NODE_KINDS(I_ENUM)
  #undef I_ENUM
  _NodeKindMax
} NodeKind;

// Lookup table N<kind> => NClass<class>
const NodeClass NodeClassTable[_NodeKindMax];

// NodeKindName returns the name of node kind constant
const char* NodeKindName(NodeKind);

// NodeClassName returns the name of a node class constant
const char* NodeClassName(NodeClass);


// Scope represents a lexical namespace
typedef struct Scope Scope;
typedef struct Scope {
  u32          childcount; // number of scopes referencing this scope as parent
  const Scope* parent;
  SymMap       bindings;
} Scope;

typedef struct Node Node;
Scope* ScopeNew(const Scope* parent, Memory);
void ScopeFree(Scope*, Memory);
const Node* ScopeAssoc(Scope*, Sym, const Node* value); // Returns replaced value or NULL
const Node* ScopeLookup(const Scope*, Sym);
const Scope* GetGlobalScope();

// NodeList is a linked list of nodes
typedef struct NodeListLink NodeListLink;
typedef struct NodeListLink {
  Node*         node;
  NodeListLink* next;
} NodeListLink;
typedef struct {
  NodeListLink* head;
  NodeListLink* tail;
  u32           len;   // number of items
} NodeList;


// type Val struct {
//   // U contains one of:
//   // bool     bool when Ctype() == CTBOOL
//   // *Mpint   int when Ctype() == CTINT, rune when Ctype() == CTRUNE
//   // *Mpflt   float when Ctype() == CTFLT
//   // *Mpcplx  pair of floats when Ctype() == CTCPLX
//   // string   string when Ctype() == CTSTR
//   // *Nilval  when Ctype() == CTNIL
//   U interface{}
// }

// func (v Val) Ctype() Ctype {
//   switch x := v.U.(type) {
//   default:
//     Fatalf("unexpected Ctype for %T", v.U)
//     panic("unreachable")
//   case nil:
//     return 0
//   case *NilVal:
//     return CTNIL
//   case bool:
//     return CTBOOL
//   case *Mpint:
//     if x.Rune {
//       return CTRUNE
//     }
//     return CTINT
//   case *Mpflt:
//     return CTFLT
//   case *Mpcplx:
//     return CTCPLX
//   case string:
//     return CTSTR
//   }
// }

typedef struct NVal {
  TypeCode t;
  union {
    u64    i;  // BoolLit, IntLit
    double f;  // FloatLit
    Str    s;  // StrLit
  };
} NVal;

typedef struct Node {
  NodeKind kind;      // kind of node (e.g. NIdent)
  SrcPos   pos;       // source origin & position
  Node*    type;      // value type. null if unknown.
  union {
    void* _never; // for initializers
    NVal val; // BoolLit, IntLit, FloatLit, StrLit
    struct { // Comment
      const u8* ptr;
      size_t    len;
    } str;
    struct { // Ident
      Sym   name;
      const Node* target;
    } ref;
    struct { // Op, PrefixOp, Return, Assign
      Node* left;
      Node* right;  // null for PrefixOp. null for Op when its a postfix op.
      Tok   op;
    } op;
    struct { // Tuple, Block, File
      Scope*   scope; // non-null if kind==Block|File
      NodeList a;
    } array;
    struct { // Fun
      Scope* scope;  // parameter scope
      Node*  params; // input parameters (result type is stored in n.type during parsing)
      Sym    name;   // null for fun-type and lambda
      Node*  body;   // null for fun-type and fun-declaration
    } fun;
    struct { // Call, TypeCast
      Node* receiver; // either an NFun or a type (e.g. NBasicType)
      Node* args;
    } call;
    struct { // Arg, Field, Let
      Sym   name;
      Node* init;  // Field: initial value (may be NULL). Let: final value (never NULL).
      u32   index; // Arg: argument index.
    } field;
    struct { // If
      Node* cond;
      Node* thenb;
      Node* elseb; // null or expr
    } cond;

    // Type
    struct {
      Sym id; // lazy; initially NULL. Computed from Node.
      union {
        struct { // BasicType
          TypeCode typeCode;
          Sym      name;
        } basic;
        NodeList tuple; // TupleType
        struct { // FunType
          Node* params;
          Node* result;
        } fun;
      };
    } t;

  }; // union
} Node;

// Node* NodeAlloc(NodeKind); // one-off allocation using calloc()
// inline static void NodeFree(Node* _) {}
Str NodeRepr(const Node* n, Str s); // return human-readable printable text representation

// NodeReprShort returns a short string representation of a node,
// suitable for use in error messages.
// Note: The returned string is valid until the next call to TmpRecycle.
const char* NodeReprShort(const Node*);

// NodeKindIsType returns true if kind represents a type (i.e. NBasicType, NFunType, etc.)
static inline bool NodeKindIsType(NodeKind kind) {
  return NodeClassTable[kind] == NodeClassType;
}

static inline bool NodeKindIsConst(NodeKind kind) {
  return NodeClassTable[kind] == NodeClassConst;
}

Str NValFmt(Str s, const NVal* v);
const char* NValStr(const NVal* v); // returns a temporary string

#define NodeListForEach(list, nodename, body)               \
  do {                                                      \
    auto __l = (list)->head;                                \
    while (__l) {                                           \
      auto nodename = __l->node;                            \
      body;                                                 \
      __l = __l->next;                                      \
    }                                                       \
  } while(0)


#define NodeListMap(list, nodename, expr)                   \
  do {                                                      \
    auto __l = (list)->head;                                \
    while (__l) {                                           \
      auto nodename = __l->node;                            \
      __l->node = expr;                                     \
      __l = __l->next;                                      \
    }                                                       \
  } while(0)


// Add node to list
void NodeListAppend(Memory mem, NodeList*, Node*);

static inline void NodeListClear(NodeList* list) {
  list->len = 0;
  list->head = NULL;
  list->tail = NULL;
}


const Node* NodeBad;  // kind==NBad

// allocate a node from an allocator
static inline Node* NewNode(Memory mem, NodeKind kind) {
  Node* n = (Node*)memalloc(mem, sizeof(Node));
  n->kind = kind;
  return n;
}

static inline Node* NodeCopy(Memory mem, const Node* src) {
  Node* n = (Node*)memalloc(mem, sizeof(Node));
  memcpy(n, src, sizeof(Node));
  return n;
}
