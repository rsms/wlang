#pragma once
#include "array.h"
#include "fwalloc.h"

typedef enum {
  NodeClassInvalid = 0,
  NodeClassExpr,
  NodeClassType,
} NodeClass;

// Primary node kinds which are either expressions or start of expressions
#define DEF_NODE_KINDS(_) \
  /* N<kind>     NClass<class> */ \
  _(None,        Invalid) \
  _(Bad,         Invalid) /* substitute "filler node" for invalid syntax */ \
  _(Type,        Type) /* Basic type, e.g. int, bool */ \
  _(FunType,     Type) /* Function type, e.g. (int,int)->(float,bool) */ \
  _(TupleType,   Type) /* Tuple type, e.g. (float,bool,int) */ \
  _(File,        Expr) \
  _(Comment,     Expr) \
  _(Ident,       Expr) \
  _(Op,          Expr) \
  _(PrefixOp,    Expr) \
  _(Nil,         Expr) /* nil literal */ \
  _(Bool,        Expr) /* boolean literal */ \
  _(Int,         Expr) /* integer literal */ \
  _(Float,       Expr) /* floating-point literal */ \
  _(Const,       Expr) \
  _(Var,         Expr) \
  _(Assign,      Expr) \
  _(Fun,         Expr) \
  _(Field,       Expr) \
  _(If,          Expr) \
  _(Return,      Expr) \
  _(Call,        Expr) \
  _(ZeroInit,    Expr) \
  _(Block,       Expr) \
  _(List,        Expr) \
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
Scope* ScopeNew(const Scope* parent);
void ScopeFree(Scope*);
const Node* ScopeAssoc(Scope*, Sym, const Node* value);
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
  // u.
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
    struct { // Op, PrefixOp, Return
      Node* left;
      Node* right;  // null for unary operations
      Tok   op;
    } op;
    struct { // List, Block, File, TupleType
      Scope*   scope; // non-null if kind==Block|File
      NodeList a;
    } array;
    struct { // Fun
      Scope* scope;  // parameter scope
      Node*  params; // input parameters (result type is stored in n.type during parsing)
      Sym    name;   // null for fun-type and lambda
      Node*  body;   // null for fun-type and fun-declaration
    } fun;
    struct { // FunType
      Node* params;
      Node* result;
    } tfun;
    struct { // Call
      Node* receiver;
      Node* args;
    } call;
    struct { // Field, Var, Const
      Sym   name;
      Node* init;  // initial value
    } field;
    struct { // If
      Node* cond;
      Node* thenb;
      Node* elseb; // null or expr
    } cond;
  } u;
} Node;

// Node* NodeAlloc(NodeKind); // one-off allocation using calloc()
// inline static void NodeFree(Node* _) {}
Str NodeRepr(const Node* n, Str s); // return human-readable printable text representation

// NodeReprShort returns a short string representation of a node,
// suitable for use in error messages.
// Important: The returned string is invalidated on the next call to NodeReprShort,
// so either copy it into an sds Str or make use of it right away.
const char* NodeReprShort(const Node*);

// NodeIsType returns true if n represents a type (i.e. NType, NFunType, etc.)
static inline bool NodeIsType(const Node* n) {
  return n != NULL && NodeClassTable[n->kind] == NodeClassType;
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
void NodeListAppend(FWAllocator* na, NodeList*, Node*);

static inline void NodeListClear(NodeList* list) {
  list->len = 0;
  list->head = NULL;
  list->tail = NULL;
}



const Node* NodeBad;  // kind==NBad

// allocate a node from an allocator
static inline Node* NewNode(FWAllocator* na, NodeKind kind) {
  Node* n = (Node*)FWAlloc(na, sizeof(Node));
  n->kind = kind;
  return n;
}
