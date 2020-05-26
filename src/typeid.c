#include "wp.h"
#include "hash.h"
#include "types.h"

// See docs/typeid.md


// static bool TypeEquals(const Node* t1, const Node* t2) {
//   return t1 == t2;
// }

// TypeSymRepr returns a printable representation of a type symbol's value.
// E.g. a tuple (int, int, bool) is represented as "(iib)".
// Not thread safe!
static const char* TypeSymRepr(const Sym s) {
  static Str buf;
  if (buf == NULL) {
    buf = sdsempty();
  } else {
    sdssetlen(buf, 0);
  }
  buf = sdscatrepr(buf, s, symlen(s));
  return buf;
}

// // TypeSymStr returns a human-readable representation of a type symbol.
// // E.g. a tuple (int, int, bool) is represented as "(int, int, bool)".
// // Not thread safe!
// static const char* TypeSymStr(const Sym s) {
//   static Str buf;
//   if (buf == NULL) {
//     buf = sdsempty();
//   } else {
//     sdssetlen(buf, 0);
//   }
//   buf = sdscat(buf, "TODO(TypeSymStr)");
//   // TODO
//   return buf;
// }


inline static void sdsPushChar(Str s, char c) {
  auto slen = sdslen(s);
  s[slen] = c;
  sdssetlen(s, slen + 1);
}


static Str buildTypeSymStr(Str s, const Node* n) {
  if (sdsavail(s) < 1) {
    s = sdsMakeRoomFor(s, 32);
  }
  switch (n->kind) {

    case NType: {
      sdsPushChar(s, n->u.t.tid);
      break;
    }

    case NTupleType: {
      sdsPushChar(s, TypeID_tuple);
      NodeListForEach(&n->u.ttuple.a, n, {
        dlog("+ append node of type %s", NodeReprShort(n));
        s = buildTypeSymStr(s, n);
      });
      if (sdsavail(s) < 1) {
        s = sdsMakeRoomFor(s, 32);
      }
      sdsPushChar(s, TypeID_tupleEnd);
      break;
    }

    default:
      dlog("TODO buildTypeSymStr handle %s", NodeKindName(n->kind));
      break;
  }
  return s;
}


// getTypeSym returns the type Sym for t.
// Not thread safe!
static Sym getTypeSym(const Node* t) {
  // TODO: precompile type Sym's for all basic types (all in TYPE_SYMS + nil)
  static Str buf;
  if (buf == NULL) {
    buf = sdsempty();
  } else {
    sdssetlen(buf, 0);
  }
  buf = buildTypeSymStr(buf, t);
  // Note: buf is likely not nil-terminated at this point. Use sdslen(buf).
  dlog("buildTypeSymStr() => %s", sdscatrepr(sdsempty(), buf, sdslen(buf)));

  return symgeth((const u8*)buf, sdslen(buf));
}


static size_t typeIDForNode(const Node* n) {
  switch (n->kind) {
    case NType: return n->u.t.tid;
    case NTupleType: return TypeID_tuple;
    default:
      dlog("TODO buildTypeSymStr handle %s", NodeKindName(n->kind));
      return TypeID_nil;
  }
}

//
// Operations needed:
//   typeEquals(a,b) -- a and b are of the same, identical type
//   typeFitsIn(a,b) -- b is subset of a (i.e. b fits in a)
//
//   Note: We do NOT need fast indexing for switches, since a variant is required
//   for switches over variable types:
//     type Thing = Error(str) | Result(u32)
//     switch thing {
//       Error("hello") => ...
//       Error(msg) => ...
//       Result(code) => ...
//     }
//
//   But hey, maybe we do for function dispatch:
//     fun log(msg str) { ... }
//     fun log(level int, msg str) { ... }
//
//   Also, some sort of matching...
//     v = (1, 2.0, true)  // typeof(v) = (int, float, bool)
//     switch v {
//       (id, _, ok) => doThing
//            ^
//          Wildcard
//     }
//     v can also be (1, "lol", true) // => (int, str, bool)
//
// To solve for all of this we use a "type symbol" — a Sym which describes the shape of a type.
//   ((int,float),(bool,int)) = "((23)(12))"
// Syms interned: testing for equality is just a pointer equality check.
// Syms are hashed and can be stored and looked-up in a Scope very effectively.
//

// ——————————————————————————————————————————————————————————————————————————————————————————————
// unit test

#ifdef DEBUG
static void test() {
  printf("--------------------------------------------------\n");

  FWAllocator mem = {0};
  FWAllocInit(&mem);
  #define mknode(t) NewNode(&mem, (t))

  auto scope = ScopeNew(GetGlobalScope()); // defined in ast.h

  dlog("sizeof(TypeID_nil) %zu", sizeof(TypeID_nil));

  // const u8 data[] = { '\t', 1, 2, 3 };
  // auto typeSym = symgeth(data, sizeof(data));
  // dlog("typeSym: %p", typeSym);

  // auto typeNode = mknode(NType);
  // dlog("typeNode: %p", typeNode);

  // const Node* replacedNode = ScopeAssoc(scope, typeSym, typeNode);
  // dlog("ScopeAssoc() => replacedNode: %p", replacedNode);

  // const Node* foundTypeNode = ScopeLookup(scope, typeSym);
  // dlog("ScopeLookup() => foundTypeNode: %p", foundTypeNode);


  { // build a type Sym from a basic data type. Type_int is a predefined node (sym.h)
    auto intType = getTypeSym(Type_int);
    dlog("int intType: %p %s", intType, TypeSymRepr(intType));
  }

  { // (int, int, bool) => "(iib)"
    Node* tupleType = mknode(NTupleType);
    NodeListAppend(&mem, &tupleType->u.ttuple.a, Type_int);
    NodeListAppend(&mem, &tupleType->u.ttuple.a, Type_int);
    NodeListAppend(&mem, &tupleType->u.ttuple.a, Type_bool);
    auto typeSym = getTypeSym(tupleType);
    dlog("tuple (int, int, bool) typeSym: %p %s", typeSym, TypeSymRepr(typeSym));
  }

  { // ((int, int), (bool, int), int) => "((ii)(bi)i)"
    Node* t2 = mknode(NTupleType);
    NodeListAppend(&mem, &t2->u.ttuple.a, Type_bool);
    NodeListAppend(&mem, &t2->u.ttuple.a, Type_int);

    Node* t1 = mknode(NTupleType);
    NodeListAppend(&mem, &t1->u.ttuple.a, Type_int);
    NodeListAppend(&mem, &t1->u.ttuple.a, Type_int);

    Node* t0 = mknode(NTupleType);
    NodeListAppend(&mem, &t0->u.ttuple.a, t1);
    NodeListAppend(&mem, &t0->u.ttuple.a, t2);
    NodeListAppend(&mem, &t0->u.ttuple.a, Type_int);

    auto typeSym = getTypeSym(t0);
    dlog("tuple (int, int, bool) typeSym: %p %s", typeSym, TypeSymRepr(typeSym));
  }


  FWAllocFree(&mem);
  printf("\n--------------------------------------------------\n\n");
}

W_UNIT_TEST(TypeID, { test(); }) // W_UNIT_TEST
#endif
