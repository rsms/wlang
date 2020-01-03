// Primary node kinds which are either expressions or start of expressions
#define DEF_NODE_KINDS_EXPR(_) \
  _(None) \
  _(Bad) /* substitute "filler node" for invalid syntax */ \
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
typedef struct Node {
  Node*     link_next; // link to next item when part of a list
  NodeKind  kind;      // kind of node (e.g. NIdent)
  SrcPos    pos;       // source origin & position
  Node*     type;      // value type. null if unknown.
  // u.
  union {
    u64    integer;  // Int
    double real;     // Float
    Sym    name;     // Ident
    struct { // Comment, String
      const u8* ptr;
      size_t    len;
    } str;
    struct { // Op, PrefixOp, Return
      Node* left;
      Node* right;  // null for unary operations
      Tok   op;
    } op;
    struct { // Const, Var
      Node* head; // null for empty list
      Node* tail; // null for empty list
    } list;
    struct { // Fun
      Node* params; // note: result type in Node.type
      Sym   name;   // null for fun-type and lambda
      Node* body;   // null for fun-type and fun-declaration
    } fun;
    struct { // Call
      Node* receiver;
      Node* args;
    } call;
    struct { // Field
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

