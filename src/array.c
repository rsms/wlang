#include "wp.h"
#include "array.h"
#include <stdlib.h> // for qsort_r

// ARRAY_CAP_STEP defines a power-of-two which the cap must be aligned to.
// This is used to round up growth. I.e. grow by 60 with a cap of 32 would increase the cap
// to 96 (= 32 + (align2(60, ARRAY_CAP_STEP=32) = 64)).
#define ARRAY_CAP_STEP 32

typedef struct SortCtx {
  ArraySortFun* f;
  void*         userdata;
} SortCtx;


static int _sort(void* ctx, const void* s1p, const void* s2p) {
  return ((SortCtx*)ctx)->f(
    *((const void**)s1p),
    *((const void**)s2p),
    ((SortCtx*)ctx)->userdata
  );
}

void ArraySort(Array* a, ArraySortFun* f, void* userdata) {
  SortCtx ctx = { f, userdata };
  qsort_r(a->v, a->len, sizeof(void*), &ctx, &_sort);
}


void ArrayGrow(Array* a, size_t addl, Memory mem) {
  u32 reqcap = a->cap + addl;
  u32 cap = align2(reqcap, ARRAY_CAP_STEP);
  if (a->onheap || a->v == NULL) {
    a->v = memrealloc(mem, a->v, sizeof(void*) * cap);
  } else {
    // moving array from stack to heap
    void** v = (void**)memalloc(mem, sizeof(void*) * cap);
    memcpy(v, a->v, sizeof(void*) * a->len);
    a->v = v;
    a->onheap = true;
  }
  a->cap = cap;
}

int ArrayIndexOf(Array* nonull a, void* nullable entry) {
  for (u32 i = 0; i < a->len; i++) {
    if (a->v[i] == entry) {
      return (int)i;
    }
  }
  return -1;
}

void ArrayRemove(Array* a, u32 start, u32 count) {
  assert(start + count <= a->len);
  // ArrayRemove( [0 1 2 3 4 5 6 7] start=2 count=3 ) => [0 1 5 6 7]
  //
  for (u32 i = start + count; i < a->len; i++) {
    a->v[i - count] = a->v[i];
  }
  // [0 1 2 3 4 5 6 7]   a->v[5-3] = a->v[5]  =>  [0 1 5 3 4 5 6 7]
  //      ^     i
  //
  // [0 1 2 3 4 5 6 7]   a->v[6-3] = a->v[6]  =>  [0 1 5 6 4 5 6 7]
  //        ^     i
  //
  // [0 1 2 3 4 5 6 7]   a->v[7-3] = a->v[7]  =>  [0 1 5 6 7 5 6 7]
  //          ^     i
  //
  // len -= count                             =>  [0 1 5 6 7]
  a->len -= count;
}


// ArrayCopy copies src of srclen to a, starting at a.v[start], growing a if needed using m.
void ArrayCopy(Array* nonull a, u32 start, const void* src, u32 srclen, Memory nullable mem) {
  u32 capNeeded = start + srclen;
  if (capNeeded > a->cap) {
    if (a->v == NULL) {
      // initial allocation to exactly the size needed
      a->v = (void*)memalloc(mem, sizeof(void*) * capNeeded);
      a->cap = capNeeded;
      a->onheap = true;
    } else {
      ArrayGrow(a, capNeeded - a->cap, mem);
    }
  }
  memcpy(&a->v[start], src, srclen * sizeof(void*));
  a->len = max(a->len, start + srclen);
}
