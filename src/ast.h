#pragma once
#include "array.h"

// Primary node kinds which are either expressions or start of expressions
#define DEF_NODE_KINDS_EXPR(_) \
  _(None) \
  _(Bad) /* substitute "filler node" for invalid syntax */ \
  _(Type) /* Built-in basic type, e.g. int, bool */ \
  _(File) \
  _(Comment) \
  _(Ident) \
  _(Op) \
  _(PrefixOp) \
  _(Bool) /* boolean literal */ \
  _(Int) /* integer literal */ \
  _(Float) /* floating-point literal */ \
  _(Const) \
  _(Var) \
  _(Assign) \
  _(Fun) \
  _(Field) \
  _(If) \
  _(Return) \
  _(Call) \
  _(ZeroInit) \
/*END DEF_NODE_KINDS_EXPR*/
// Compound node kinds
#define DEF_NODE_KINDS(_) \
  _(Block) \
  _(List) \
/*END DEF_NODE_KINDS*/

typedef enum {
  #define I_ENUM(name) N##name,
  DEF_NODE_KINDS_EXPR(I_ENUM)
  #undef I_ENUM
  #define I_ENUM(name) N##name,
  DEF_NODE_KINDS(I_ENUM)
  #undef I_ENUM
} NodeKind;


typedef struct Node Node;


// Scope represents a lexical namespace
typedef struct Scope Scope;
typedef struct Scope {
  u32          childcount; // number of scopes referencing this scope as parent
  const Scope* parent;
  SymMap       bindings;
} Scope;

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
      Node*  params; // note: result type in Node.type
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

Node* NodeAlloc(NodeKind);
inline static void NodeFree(Node* _) {} // just leak for now
const char* NodeKindName(NodeKind);
Str NodeRepr(const Node* n, Str s);

const Node* NodeBad;  // kind==NBad
