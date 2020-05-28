#include "../wp.h"
#include "ir.h"

#define RBKEY      intptr_t
#define RBKEY_NULL 0
#define RBVALUE    void*
#define RBUSERDATA void*
#include "../rbtree.c"

// ———————————————————————————————————————————————————————————————————————————————————————————————
// RB API

inline static RBNode* RBAllocNode(void* userdata) {
  auto n = (RBNode*)FWAlloc((FWAllocator*)userdata, sizeof(RBNode));
  // return (RBNode*)malloc(sizeof(RBNode));
  // dlog("** RBAllocNode %p  (%zu B)", n, sizeof(RBNode));
  return n;
}

inline static void RBFreeNode(RBNode* node, void* userdata) {
  // Do nothing since node was allocated by fwalloc and will be gc'd later
  // free(node);
  // dlog("** RBFreeNode %p", node);
}

inline static void RBFreeValue(void* value, void* userdata) {
  // Called when a value is removed or replaced.
  // Do nothing since value was allocated by fwalloc and will be gc'd later
}

inline static int RBCmp(intptr_t a, intptr_t b, void* userdata) {
  if (a < b) { return -1; }
  if (b < a) { return 1; }
  return 0;
}

#ifdef _DOCUMENTATION_ONLY_
// RBHas performs a lookup of k. Returns true if found.
bool RBHas(const RBNode* n, RBKEY k, void* userdata);

// RBGet performs a lookup of k. Returns value or RBVALUE_NOT_FOUND.
RBVALUE RBGet(const RBNode* n, RBKEY k, void* userdata);

RBNode* RBGetNode(RBNode* node, RBKEY key, void* userdata);

// RBSet adds or replaces value for k. Returns new n.
RBNode* RBSet(RBNode* n, RBKEY k, RBVALUE v, void* userdata);

// RBSet adds value for k if it does not exist. Returns new n.
// "added" is set to true when a new value was added, false otherwise.
RBNode* RBAdd(RBNode* n, RBKEY k, RBVALUE v, bool* added, void* userdata);

// RBDelete removes k if found. Returns new n.
RBNode* RBDelete(RBNode* n, RBKEY k, void* userdata);

// RBClear removes all entries. n is invalid after this operation.
void RBClear(RBNode* n, void* userdata);

// Iteration. Return true from callback to keep going.
typedef bool(RBIterator)(const RBNode* n, void* userdata);
bool RBIter(const RBNode* n, RBIterator* f, void* userdata);

// RBCount returns the number of entries starting at n. O(n) time complexity.
size_t RBCount(const RBNode* n);

// RBRepr formats n as printable lisp text, useful for inspecting a tree.
// keystr should produce a string representation of a given key.
Str RBRepr(const RBNode* n, Str s, int depth, Str(keyfmt)(Str,RBKEY));
#endif
// ———————————————————————————————————————————————————————————————————————————————————————————————
//
// const cache is structured in two levels, like this:
//
// type -> RBNode { value -> IRValue }
//
/*

typedef struct IRConstCache {
  u32   bmap;       // maps TypeCode => branch array index
  void* branches[]; // dense branch array
} IRConstCache;
*/


// number of entries in c->entries
inline static u32 branchesLen(const IRConstCache* c) {
  // int __builtin_popcount(int)
  return (u32)__builtin_popcount(c->bmap);
}

// bitindex ... [TODO doc]
inline static u32 bitindex(u32 bmap, u32 bitpos) {
  return (u32)__builtin_popcount(bmap & (bitpos - 1));
}


inline static IRConstCache* IRConstCacheAlloc(FWAllocator* a, size_t entryCount) {
  return (IRConstCache*)FWAlloc(a, sizeof(IRConstCache) + (entryCount * sizeof(void*)));
}

// static const char* fmtbin(u32 n) {
//   static char buf[33];
//   u32 i = 0;
//   while (n) {
//     buf[i++] = n & 1 ? '1' : '0';
//     n >>= 1;
//   }
//   buf[i] = '\0';
//   return buf;
// }


IRValue* IRConstCacheGet(
  const IRConstCache* c,
  FWAllocator* a,
  TypeCode t,
  intptr_t value,
  int* out_addHint
) {
  if (c != NULL) {
    u32 bitpos = 1 << t;
    if ((c->bmap & bitpos) != 0) {
      u32 bi = bitindex(c->bmap, bitpos); // index in c->buckets
      auto branch = (RBNode*)c->branches[bi];
      assert(branch != NULL);
      if (out_addHint != NULL) {
        *out_addHint = (int)(bi + 1);
      }
      return (IRValue*)RBGet(branch, value, a);
    }
  }
  if (out_addHint != NULL) {
    *out_addHint = 0;
  }
  return NULL;
}

IRConstCache* IRConstCacheAdd(
  IRConstCache* c,
  FWAllocator* a,
  TypeCode t,
  intptr_t value,
  IRValue* v,
  int addHint
) {
  // Invariant: t>=0 and t<32
  // Note: TypeCode_NUM_END is static_assert to be <= 32

  // dlog("IRConstCacheAdd type=%c (%u) value=0x%lX", TypeCodeEncoding[t], t, value);

  // if addHint is not NULL, it is the branch index+1 of the type branch
  if (addHint > 0) {
    u32 bi = (u32)(addHint - 1);
    auto branch = (RBNode*)c->branches[bi];
    c->branches[bi] = RBSet(branch, value, v, a);
    return c;
  }

  const u32 bitpos = 1 << t;

  if (c == NULL) {
    // first type tree
    // dlog("case A -- initial branch");
    c = IRConstCacheAlloc(a, 1);
    c->bmap = bitpos;
    c->branches[0] = RBSet(NULL, value, v, a);
  } else {
    u32 bi = bitindex(c->bmap, bitpos); // index in c->buckets
    if ((c->bmap & bitpos) == 0) {
      // dlog("case B -- new branch");
      // no type tree -- copy c into a +1 sized memory slot
      auto nbranches = branchesLen(c);
      auto c2 = IRConstCacheAlloc(a, nbranches + 1);
      c2->bmap = c->bmap | bitpos;
      auto dst = &c2->branches[0];
      auto src = &c->branches[0];
      // copy entries up until bi
      memcpy(dst, src, bi * sizeof(void*));
      // add bi
      dst[bi] = RBSet(NULL, value, v, a);
      // copy entries after bi
      memcpy(dst + (bi + 1), src + bi, (nbranches - bi) * sizeof(void*));
      // Note: FWAllocator is forward only so no free(c) here
      c = c2;
    } else {
      // dlog("case C -- existing branch");
      auto branch = (RBNode*)c->branches[bi];
      c->branches[bi] = RBSet(branch, value, v, a);
    }
  }

  return c;
}


// ——————————————————————————————————————————————————————————————————————————————————————————————
// unit test
#if DEBUG

static void test() {
  // printf("--------------------------------------------------\n");
  FWAllocator a = {0};
  FWAllocInit(&a);

  IRConstCache* c = NULL;
  intptr_t testValueGen = 1; // IRValue pointer simulator (generator)

  // c is null; get => null
  auto v1 = IRConstCacheGet(c, &a, TypeCode_int8, 1, 0);
  assert(v1 == NULL);

  auto expect1 = testValueGen++;
  auto expect2 = testValueGen++;
  auto expect3 = testValueGen++;

  // add values. This data causes all cases of the IRConstCacheAdd function to be used.
  // 1. initial branch creation, when c is null
  c = IRConstCacheAdd(c, &a, TypeCode_int8,  1, (IRValue*)expect1, 0);
  // 2. new branch on existing c
  c = IRConstCacheAdd(c, &a, TypeCode_int16, 1, (IRValue*)expect2, 0);
  // 3. new value on existing branch
  c = IRConstCacheAdd(c, &a, TypeCode_int16, 2, (IRValue*)expect3, 0);

  // verify that Get returns the expected values
  v1 = IRConstCacheGet(c, &a, TypeCode_int8, 1, 0);
  assert((uintptr_t)v1 == expect1);
  auto v2 = IRConstCacheGet(c, &a, TypeCode_int16, 1, 0);
  assert((uintptr_t)v2 == expect2);
  auto v3 = IRConstCacheGet(c, &a, TypeCode_int16, 2, 0);
  assert((uintptr_t)v3 == expect3);

  // test the addHint, which is an RBNode of the type branch when it exists.
  int addHint = 0;
  auto expect4 = testValueGen++;
  auto v4 = IRConstCacheGet(c, &a, TypeCode_int16, 3, &addHint);
  assert(v4 == NULL);
  assert(addHint != 0); // since TypeCode_int16 branch should exist
  c = IRConstCacheAdd(c, &a, TypeCode_int16, 3, (IRValue*)expect4, addHint);
  v4 = IRConstCacheGet(c, &a, TypeCode_int16, 3, &addHint);
  assert((uintptr_t)v4 == expect4);


  FWAllocFree(&a);
  // printf("--------------------------------------------------\n");
}
W_UNIT_TEST(IRConstCache, { test(); }) // W_UNIT_TEST
#endif
