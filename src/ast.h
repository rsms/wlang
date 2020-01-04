#pragma once

// Primary node kinds which are either expressions or start of expressions
#define DEF_NODE_KINDS_EXPR(_) \
  _(None) \
  _(Bad) /* substitute "filler node" for invalid syntax */ \
  _(File) \
  _(Comment) \
  _(Ident) \
  _(Op) \
  _(PrefixOp) \
  _(Const) \
  _(Var) \
  _(Int) \
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
typedef struct Scope {
  u32           childcount; // number of scopes referencing this scope as parent
  struct Scope* parent;
  SymMap        bindings;
} Scope;

Scope* ScopeNew();
void ScopeFree(Scope*);
Node* ScopeLookup(Scope*, Sym);


typedef struct Node {
  Node*     link_next; // link to next item when part of a list
  NodeKind  kind;      // kind of node (e.g. NIdent)
  SrcPos    pos;       // source origin & position
  Node*     type;      // value type. null if unknown.
  // u.
  union {
    u64    integer;  // Int
    double real;     // Float
    struct { // Comment, String
      const u8* ptr;
      size_t    len;
    } str;
    struct { // Ident
      Sym   name;
      Node* target;
    } ref;
    struct { // Op, PrefixOp, Return
      Node* left;
      Node* right;  // null for unary operations
      Tok   op;
    } op;
    struct { // List, Block, File
      Node*  head;  // null for empty list
      Node*  tail;  // null for empty list
      Scope* scope; // non-null if kind==Block|File
    } list;
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

const char* NodeKindName(NodeKind);
Str NodeRepr(const Node* n, Str s);
