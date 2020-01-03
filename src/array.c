#include "wp.h"
#include "array.h"

void ArrayGrow(Array* a, size_t addl) {
  u32 reqcap = a->len + addl;
  if (a->cap < reqcap) {
    u32 cap = align2(reqcap, 64);
    // u32 cap = a->cap == 0 ? 64 : a->cap * 2;
    if (a->onstack) {
      // moving array from stack to heap
      void** v = (void*)malloc(sizeof(void*) * cap);
      memcpy(v, a->v, sizeof(void*) * a->len);
      a->v = v;
      a->onstack = false;
    } else {
      a->v = (void*)realloc(a->v, sizeof(void*) * cap);
    }
    a->cap = cap;
  }
}
