#include "wp.h"
#include "strmap.h"
#include "assert.h"

#define RBKEY HStr
#include "rbtree.c"

inline static RBNode* RBAllocNode() { return (RBNode*)malloc(sizeof(RBNode)); }
inline static void RBFreeNode(RBNode* node) { free(node); }
inline static int RBCmp(HStr a, HStr b) { return HStrCmp(a, b); }

void HStrSetInit(HStrSet* s) {
  s->root = NULL;
}
void HStrSetClear(HStrSet* s) {
  if (s->root) {
    RBClear(s->root);
    s->root = NULL;
  }
}
int HStrSetHas(const HStrSet* s, HStr k) {
  return RBHas(s->root, k);
}
void HStrSetSet(HStrSet* s, HStr k) {
  s->root = RBSet(s->root, k);
}
bool HStrSetAdd(HStrSet* s, HStr k) {
  bool added;
  s->root = RBAdd(s->root, k, &added);
  return added;
}
void HStrSetDelete(HStrSet* s, HStr k) {
  s->root = RBDelete(s->root, k);
}
size_t HStrSetSize(const HStrSet* s) {
  return RBCount(s->root);
}

static Str fmtkey(HStr k) { return k; } // HStr is a type of Str :-)
Str HStrSetRepr(const HStrSet* s, Str s) {
  return RBRepr(s->root, s, 0, &fmtkey);
}


__attribute__((constructor)) static void test() {
  dlog("TEST RB");

  #define MK_HSTR(cstr) (({ \
    size_t len = strlen(cstr); \
    HStrNew(HashFNV1a((const u8*)(cstr), len), (const u8*)cstr, (u32)len); \
  }))

  auto s_hello  = MK_HSTR("hello");
  auto s_helios = MK_HSTR("helios");
  auto s_world  = MK_HSTR("world");
  dlog("hello:  %zu \"%s\" %x", HStrLen(s_hello),  s_hello,  HStrHash(s_hello));
  dlog("helios: %zu \"%s\" %x", HStrLen(s_helios), s_helios, HStrHash(s_helios));
  dlog("world:  %zu \"%s\" %x", HStrLen(s_world),  s_world,  HStrHash(s_world));

  HStrSet m;
  HStrSetInit(&m);

  HStrSetSet(&m, MK_HSTR("hello"), 1);
  HStrSetSet(&m, MK_HSTR("helios"), 2);
  HStrSetSet(&m, MK_HSTR("world"), 3);
  HStrSetSet(&m, MK_HSTR("hi"), 4);
  HStrSetSet(&m, MK_HSTR("hola"), 5);
  HStrSetSet(&m, MK_HSTR("hole"), 6);

  auto val = HStrSetGet(&m, MK_HSTR("helios"));
  dlog("HStrSetGet(helios) => %d", val);

  printf("%s\n", HStrSetRepr(&m, sdsempty()));

  dlog("HStrSetDelete(hello)");
  HStrSetDelete(&m, MK_HSTR("hello"));

  printf("%s\n", HStrSetRepr(&m, sdsempty()));

  val = HStrSetGet(&m, MK_HSTR("hello"));
  dlog("HStrSetGet(hello) => %d", val);

  bool added = HStrSetAdd(&m, MK_HSTR("hole"), 7);
  dlog("HStrSetAdd(hole) => %d", added);

  added = HStrSetAdd(&m, MK_HSTR("holy"), 7);
  dlog("HStrSetAdd(holy) => %d", added);

  assert(HStrSetAdd(&m, MK_HSTR("helios"), 2) == false);
  assert(HStrSetAdd(&m, MK_HSTR("world"), 3) == false);
  assert(HStrSetAdd(&m, MK_HSTR("hi"), 4) == false);
  assert(HStrSetAdd(&m, MK_HSTR("hola"), 5) == false);
  assert(HStrSetAdd(&m, MK_HSTR("hole"), 6) == false);

  printf("%s\n", HStrSetRepr(&m, sdsempty()));
}
