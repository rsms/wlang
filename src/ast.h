#pragma once
#include "array.h"
#include "memory.h"

typedef enum {
  NodeClassInvalid = 0,
  NodeClassConst,
  NodeClassExpr,
  NodeClassType,
} NodeClass;

// Primary node kinds which are either expressions or start of expressions
#define DEF_NODE_KINDS(_) \
  /* N<kind>     NClass<class> */ \
  _(None,        Invalid) \
  _(Bad,         Invalid) /* substitute "filler node" for invalid syntax */ \
  _(Assign,      Expr) \
  _(BasicType,   Type) /* Basic type, e.g. int, bool */ \
  _(Block,       Expr) \
  _(Bool,        Const) /* boolean literal */ \
  _(Call,        Expr) \
  _(Comment,     Expr) \
  _(Const,       Expr) \
  _(Field,       Expr) \
  _(File,        Expr) \
  _(Float,       Expr) /* floating-point literal */ \
  _(Fun,         Expr) \
  _(FunType,     Type) /* Function type, e.g. (int,int)->(float,bool) */ \
  _(Ident,       Expr) \
  _(If,          Expr) \
  _(Int,         Const) /* integer literal */ \
  _(Let,         Expr) \
  _(Nil,         Const) /* nil literal */ \
  _(Op,          Expr) \
  _(PrefixOp,    Expr) \
  _(Return,      Expr) \
  _(Tuple,       Expr) \
  _(TupleType,   Type) /* Tuple type, e.g. (float,bool,int) */ \
  _(Var,         Expr) \
  _(ZeroInit,    Expr) \
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

typedef struct Node {
  NodeKind kind;      // kind of node (e.g. NIdent)
  SrcPos   pos;       // source origin & position
  Node*    type;      // value type. null if unknown.
  union {
    u64    integer;  // Bool, Int
    double real;     // Float
    struct { // Comment, String
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
    struct { // Call
      Node* receiver;
      Node* args;
    } call;
    struct { // Field, Var, Const, Let
      Sym   name;
      Node* init;  // initial value (final value for Let)
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

// NodeIsType returns true if n represents a type (i.e. NBasicType, NFunType, etc.)
static inline bool NodeIsType(const Node* n) {
  return n != NULL && NodeClassTable[n->kind] == NodeClassType;
}

static inline bool NodeKindIsConst(NodeKind kind) {
  return NodeClassTable[kind] == NodeClassConst;
}


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
