#include "wp.h"
#include "array.h"

void ArrayGrow(Array* a, size_t addl, FWAllocator* allocator) {
  u32 reqcap = a->len + addl;
  if (a->cap < reqcap) {
    u32 cap = align2(reqcap, 64);
    // u32 cap = a->cap == 0 ? 64 : a->cap * 2;
    if (a->onheap || a->v == NULL) {
      if (allocator != NULL) {
        a->v = FWRealloc(allocator, a->v, a->cap, sizeof(void*) * cap);
      } else {
        a->v = realloc(a->v, sizeof(void*) * cap);
      }
    } else {
      // moving array from stack to heap
      void** v;
      if (allocator != NULL) {
        v = (void*)FWAlloc(allocator, sizeof(void*) * cap);
      } else {
        v = (void*)malloc(sizeof(void*) * cap);
      }
      memcpy(v, a->v, sizeof(void*) * a->len);
      a->v = v;
      a->onheap = true;
    }
    a->cap = cap;
  }
}
