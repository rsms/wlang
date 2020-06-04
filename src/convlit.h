#pragma once

// convlit converts an expression to type t.
// If expr is already of type t, expr is simply returned.
// CCtx is used for error reporting.

// For explicit conversions, t must be non-nil.
static Node* ConvlitExplicit(CCtx* cc, Node* expr, Node* t);

// For implicit conversions (e.g., assignments), t may be nil;
// if so, expr is converted to its default type.
static Node* ConvlitImplicit(CCtx* cc, Node* expr, Node* t);


Node* _convlit(CCtx* cc, Node* expr, Node* t, bool explicit);
inline static Node* ConvlitExplicit(CCtx* cc, Node* expr, Node* t) {
  return _convlit(cc, expr, t, /*explicit*/ true);
}
inline static Node* ConvlitImplicit(CCtx* cc, Node* expr, Node* t) {
  return _convlit(cc, expr, t, /*explicit*/ false);
}
