#include "wp.h"
#include "ptrmap.h"
#include "hash.h"
#include <math.h> /* log2 */
#include <limits.h> /* *_MAX */

#if ((ULONG_MAX) > (UINT_MAX))
  // 64-bit address
  #define ptrhash(ptr) ((size_t)hashFNV1a64((const u8*)&(ptr), 8))
#else
  // 32-bit address
  #define ptrhash(ptr) ((size_t)hashFNV1a((const u8*)&(ptr), 4))
#endif

// This is a good and very fast hash function for small sets of sequential pointers,
// but as the address space grows the distribution worsens quickly compared to FNV1a.
// static size_t ptrhash2(void* p) {
//   // Note: the log2 call is eliminated and replaced by a constant when compiling
//   // with optimizations.
//   const size_t shift = (size_t)log2(1 + sizeof(void*));
//   return (size_t)(p) >> shift;
// }

// hashmap implementation
#define HASHMAP_NAME     PtrMap
#define HASHMAP_KEY      void*
#define HASHMAP_VALUE    void*
#define HASHMAP_KEY_HASH ptrhash
#include "hashmap.c"
#undef HASHMAP_NAME
#undef HASHMAP_KEY
#undef HASHMAP_VALUE
#undef HASHMAP_KEY_HASH


#if DEBUG
static void testMapIterator(void* key, void* value, bool* stop, void* userdata) {
  // dlog("\"%s\" => %zu", key, (size_t)value);
  size_t* n = (size_t*)userdata;
  (*n)++;
}
#endif


W_UNIT_TEST(PtrMap, {
  auto mem = MemoryNew(0);
  auto m = PtrMapNew(8, mem);

  assert(m->len == 0);

  #define SYM(cstr) symgeth((const u8*)(cstr), strlen(cstr))
  void* oldval;

  oldval = PtrMapSet(m, "hello", (void*)1);
  // dlog("PtrMapSet(hello) => %zu", (size_t)oldval);
  assert(m->len == 1);

  oldval = PtrMapSet(m, "hello", (void*)2);
  // dlog("PtrMapSet(hello) => %zu", (size_t)oldval);
  assert(m->len == 1);

  assert(PtrMapDel(m, "hello") == (void*)2);
  assert(m->len == 0);

  size_t n = 100;
  PtrMapSet(m, "break",       (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "case",        (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "const",       (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "continue",    (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "default",     (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "defer",       (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "else",        (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "enum",        (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "fallthrough", (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "for",         (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "fun",         (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "go",          (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "if",          (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "import",      (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "in",          (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "interface",   (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "is",          (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "return",      (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "select",      (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "struct",      (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "switch",      (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "symbol",      (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "type",        (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "var",         (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "while",       (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "_",           (void*)n++); assert(m->len == n - 100);
  PtrMapSet(m, "int",         (void*)n++); assert(m->len == n - 100);

  // // print distribution of load on each bucket
  // printf("bucket,load\n");
  // u32* vals = hashmapDebugDistr(m);
  // for (u32 i = 0; i < m.cap; i++) {
  //   printf("%u,%u\n", i+1, vals[i]);
  // }
  // free(vals);

  // counts
  n = 0;
  PtrMapIter(m, testMapIterator, &n);
  assert(n == 27);

  // del
  assert(PtrMapSet(m, "hello", (void*)2) == NULL);
  assert(PtrMapGet(m, "hello") == (void*)2);
  assert(PtrMapDel(m, "hello") == (void*)2);
  assert(PtrMapGet(m, "hello") == NULL);
  assert(PtrMapSet(m, "hello", (void*)2) == NULL);
  assert(PtrMapGet(m, "hello") == (void*)2);

  PtrMapFree(m);
  MemoryFree(mem);
}) // W_UNIT_TEST
