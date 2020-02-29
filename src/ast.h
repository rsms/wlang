#pragma once
#include "array.h"

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
  _(File,        Expr) \
  _(Comment,     Expr) \
  _(Ident,       Expr) \
  _(Op,          Expr) \
  _(PrefixOp,    Expr) \
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

// Get name of node kind constant
const char* NodeKindName(NodeKind);

// Get name of node class constant
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


typedef struct Node {
  Node*       link_next; // link to next item when part of a list
  NodeKind    kind;      // kind of node (e.g. NIdent)
  SrcPos      pos;       // source origin & position
  const Node* type;      // value type. null if unknown.
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
    struct { // List, Block, File
      Array  a;           // null for empty list
      void*  astorage[2]; // initial storage of a (TODO: tune this constant)
      Scope* scope;       // non-null if kind==Block|File
    } array;
    struct { // Fun
      Node*  params; // input parameters
      Node*  result; // output result
      Sym    name;   // null for fun-type and lambda
      Node*  body;   // null for fun-type and fun-declaration
      Scope* scope;  // parameter scope
    } fun;
    struct { // Call
      Node* receiver;
      Node* args;
    } call;
    struct { // Field, Var, Const
      Sym   name;
      Node* init;  // initial value; null if none
    } field;
    struct { // If
      Node* cond;
      Node* thenb;
      Node* elseb; // null or expr
    } cond;
  } u;
} Node;

Node* NodeAlloc(NodeKind); // one-off allocation using calloc()
inline static void NodeFree(Node* _) {}
Str NodeRepr(const Node* n, Str s); // return human-readable printable text representation

const Node* NodeBad;  // kind==NBad


// NodeAllocator is a forward-only allocator for nodes.
//
// The way this works is that nodes are allocated as needed but freed in one big sweep,
// when an AST build with a NodeAllocator is no longer needed.
// Sort of like a cheapo garbage collector.
// This simplifies the code (since we do a lot of node rewiring during resolve) and speeds
// up parsing (since we don't need to free nodes during parsing.)
//
// The allocator makes use of blocks (NAllocBlock) which is a big chunk of memory.
// When a block runs out of space, a new block is allocated from the system and made the
// head (mfree) of the linked list. The used-up block is moved to the mused linked-list.
//
// NodeAllocatorInit either:
//   if used, moves all blocks from mused to mfree and resets the blocks' offs.
//   else allocates one new block on mfree.
// NodeAllocatorFree frees all blocks (mfree and mused lists) are freed.
// NAlloc allocates space for a Node in the head block of the mfree list.
//
typedef struct NAllocBlock NAllocBlock;
typedef struct NAllocBlock {
  NAllocBlock* next; // linked list
  uint offs;         // offset in data to free memory
  u8   data[];       // start of memory
} NAllocBlock;
typedef struct {
  NAllocBlock* mfree; // free memory linked-list head.
  NAllocBlock* mused; // used memory linked-list head.
} NodeAllocator;

void NodeAllocatorInit(NodeAllocator*);  // initialize new or reset existing for reuse.
void NodeAllocatorFree(NodeAllocator*); // free all its node memory
Node* NAlloc(NodeAllocator*, NodeKind);  // allocate a node from an allocator
