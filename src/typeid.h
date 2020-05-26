#pragma once
// requires: types, ast

// GetTypeID retrieves the TypeID for the type node n.
// This function may mutate n by computing and storing id to n.t.id.
Sym GetTypeID(Node* n);

// TypeEquals returns true if a and b are equivalent types (i.e. identical).
bool TypeEquals(Node* a, Node* b);
