#pragma once
#include "memory.h"

// very simple array type
typedef struct {
  void** v;
  u32    len;
  u32    cap;
  bool   onheap;  // false if v is space on stack
} Array;

#define Array_INIT { NULL, 0, 0, true }

#define Array_STACK_INIT(capacity) (({ \
  void* __ArrayStackStorage__##__LINE__[capacity]; \
  Array a = { __ArrayStackStorage__##__LINE__, 0, (capacity), false }; \
  a; \
}))

inline static void ArrayInitWithStorage(Array* a, void* storagePtr, u32 storageSize) {
  a->v = storagePtr;
  a->len = 0;
  a->cap = storageSize;
  a->onheap = false;
}

// static void ArrayInit(Array* a) {
//   a->v = NULL;
//   a->cap = a->len = 0;
//   a->onheap = true;
// }

// static void ArrayFree(Array* a) {
//   if (a->onheap) {
//     free(a->v);
//     a->v = NULL;
//     a->cap = 0;
//   }
// }

// cap = align2(len + addl)
void ArrayGrow(Array*, size_t addl, Memory mem /* nullable */);

inline static void ArrayPush(Array* a, void* v, Memory mem /* nullable */) {
  if (a->len == a->cap) {
    ArrayGrow(a, 1, mem);
  }
  a->v[a->len++] = v;
}

inline static void* ArrayPop(Array* a) {
  return a->len > 0 ? a->v[--a->len] : NULL;
}


#define ArrayForEach(a, elemtype, elemname, body)  \
  do { for (u32 i = 0; i < (a)->len; i++) {        \
    elemtype elemname = (elemtype)(a)->v[i];       \
    body;                                          \
  } } while(0)
