#include "wp.h"
#include "strmap.h"
#include "assert.h"

#define RBKEY   HStr
#define RBVALUE void*
#include "rbtree.c"

inline static RBNode* RBAllocNode() { return (RBNode*)malloc(sizeof(RBNode)); }
inline static void RBFreeNode(RBNode* node) { free(node); }
inline static void RBFreeValue(RBVALUE v) {}
inline static int RBCmp(HStr a, HStr b) { return HStrCmp(a, b); }

void StrMapInit(StrMap* m) {
  m->root = NULL;
}
void StrMapClear(StrMap* m) {
  if (m->root) {
    RBClear(m->root);
    m->root = NULL;
  }
}
RBVALUE StrMapGet(const StrMap* m, HStr k) {
  return m->root ? RBGet(m->root, k) : 0;
}
void StrMapSet(StrMap* m, HStr k, RBVALUE v) {
  m->root = RBSet(m->root, k, v);
}
bool StrMapAdd(StrMap* m, HStr k, RBVALUE v) {
  bool added;
  m->root = RBAdd(m->root, k, v, &added);
  return added;
}
void StrMapDelete(StrMap* m, HStr k) {
  m->root = RBDelete(m->root, k);
}
size_t StrMapLen(const StrMap* m) {
  return m->root ? RBCount(m->root) : 0;
}
bool StrMapEmpty(const StrMap* m) {
  return m->root == NULL;
}

static Str fmtkey(HStr k) { return k; } // HStr is a type of Str :-)
Str StrMapRepr(const StrMap* m, Str s) {
  return m->root ? RBRepr(m->root, s, 0, &fmtkey) : s;
}

#if 0
__attribute__((constructor)) static void test() {

  #define MK_HSTR(cstr) (({ \
    size_t len = strlen(cstr); \
    HStrNew(HashFNV1a((const u8*)(cstr), len), (const u8*)cstr, (u32)len); \
  }))

  // auto s_hello  = MK_HSTR("hello");
  // auto s_helios = MK_HSTR("helios");
  // auto s_world  = MK_HSTR("world");
  // dlog("hello:  %zu \"%s\" %x", HStrLen(s_hello),  s_hello,  HStrHash(s_hello));
  // dlog("helios: %zu \"%s\" %x", HStrLen(s_helios), s_helios, HStrHash(s_helios));
  // dlog("world:  %zu \"%s\" %x", HStrLen(s_world),  s_world,  HStrHash(s_world));

  StrMap m = StrMap_INIT;
  assert(StrMapEmpty(&m));
  assert(StrMapLen(&m) == 0);

  StrMapSet(&m, MK_HSTR("hello"), (RBVALUE)1);
  assert(StrMapEmpty(&m) == false);
  assert(StrMapLen(&m) == 1);
  assert(StrMapGet(&m, MK_HSTR("hello")) == (RBVALUE)1);

  StrMapSet(&m, MK_HSTR("helios"), (RBVALUE)2);  assert(StrMapLen(&m) == 2);
  assert(StrMapGet(&m, MK_HSTR("helios")) == (RBVALUE)2);
  StrMapSet(&m, MK_HSTR("world"),  (RBVALUE)3);  assert(StrMapLen(&m) == 3);
  assert(StrMapGet(&m, MK_HSTR("world")) == (RBVALUE)3);
  StrMapSet(&m, MK_HSTR("hi"),     (RBVALUE)4);  assert(StrMapLen(&m) == 4);
  assert(StrMapGet(&m, MK_HSTR("hi")) == (RBVALUE)4);
  StrMapSet(&m, MK_HSTR("hola"),   (RBVALUE)5);  assert(StrMapLen(&m) == 5);
  assert(StrMapGet(&m, MK_HSTR("hola")) == (RBVALUE)5);
  StrMapSet(&m, MK_HSTR("hole"),   (RBVALUE)6);  assert(StrMapLen(&m) == 6);
  assert(StrMapGet(&m, MK_HSTR("hole")) == (RBVALUE)6);

  StrMapDelete(&m, MK_HSTR("hello"));
  assert(StrMapGet(&m, MK_HSTR("hello")) == NULL);

  // dlog("StrMapLen: %zu", StrMapLen(&m));
  // auto val = StrMapGet(&m, MK_HSTR("helios"));
  // dlog("StrMapGet(helios) => %d", val);
  // printf("%s\n", StrMapRepr(&m, sdsempty()));
  // dlog("StrMapDelete(hello)");
  // StrMapDelete(&m, MK_HSTR("hello"));
  // printf("%s\n", StrMapRepr(&m, sdsempty()));
  // val = StrMapGet(&m, MK_HSTR("hello"));
  // dlog("StrMapGet(hello) => %d", val);

  assert(StrMapAdd(&m, MK_HSTR("hole"), (RBVALUE)6) == false);
  assert(StrMapAdd(&m, MK_HSTR("holy"), (RBVALUE)7) == true);

  assert(StrMapAdd(&m, MK_HSTR("helios"), (RBVALUE)2) == false);
  assert(StrMapAdd(&m, MK_HSTR("world"),  (RBVALUE)3) == false);
  assert(StrMapAdd(&m, MK_HSTR("hi"),     (RBVALUE)4) == false);
  assert(StrMapAdd(&m, MK_HSTR("hola"),   (RBVALUE)5) == false);
  assert(StrMapAdd(&m, MK_HSTR("hole"),   (RBVALUE)6) == false);

  // printf("%s\n", StrMapRepr(&m, sdsempty()));

  assert(StrMapEmpty(&m) == false);
  StrMapClear(&m);
  assert(StrMapEmpty(&m) == true);
}
#endif
