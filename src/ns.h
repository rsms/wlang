#pragma once
#include "sym.h"

// Namespace maps names to nodes.
// .bindings are filled in during parsing and accessed during evaluation (for looking up symbols).
typedef struct Namespace {
  Str    qname;    // Qualified name, i.e. "user.foo.bar.". Always ends in "."
  SymMap bindings; // Unqualified names to expressions defined in this namespace.
} Namespace;
