#pragma once
#include "scan.h"
#include "../common/array.h"
#include "ast.h"
// #include "common/assert.h"
// #include "common/test.h"
// #include "common/memory.h"
// #include "common/str.h"
// #include "common/os.h"
// #include "sym.h"

// parser
typedef struct P {
  S      s;          // scanner
  u32    fnest;      // function nesting level (for error handling)
  u32    unresolved; // number of unresolved identifiers
  Scope* scope;      // current scope
  CCtx*  cc;         // compilation context
} P;
Node* Parse(P*, CCtx*, ParseFlags, Scope* pkgscope);
Node* NodeOptIfCond(Node* n); // TODO: move this and parser into a parse.h file

// Symbol resolver
Node* ResolveSym(CCtx*, ParseFlags, Node*, Scope*);

// Type resolver
void ResolveType(CCtx*, Node*);
