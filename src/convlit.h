#pragma once

// convlit converts an expression to type t.
// If n is already of type t, n is simply returned.
// CCtx is used for error reporting.

// For explicit conversions, which allows a greater range of conversions.
static Node* ConvlitExplicit(CCtx* cc, Node* n, Node* t);

// For implicit conversions (e.g. operands)
static Node* ConvlitImplicit(CCtx* cc, Node* n, Node* t);


Node* convlit(CCtx* cc, Node* n, Node* t, bool explicit);
inline static Node* ConvlitExplicit(CCtx* cc, Node* n, Node* t) {
  return convlit(cc, n, t, /*explicit*/ true);
}
inline static Node* ConvlitImplicit(CCtx* cc, Node* n, Node* t) {
  return convlit(cc, n, t, /*explicit*/ false);
}
