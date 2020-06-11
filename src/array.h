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

static void  ArrayInitWithStorage(Array* nonull a, void* nonull storagePtr, u32 storageSize);
static void  ArrayFree(Array* nonull a, Memory nullable mem);
void         ArrayGrow(Array* nonull a, size_t addl, nullable Memory mem); // cap=align2(len+addl)
static void  ArrayPush(Array* nonull a, void* nullable v, Memory nullable mem);
static void* ArrayPop(Array* nonull a);
void         ArrayRemove(Array* nonull a, u32 start, u32 count);
int          ArrayIndexOf(Array* nonull a, void* nullable entry); // -1 on failure

// ArrayCopy copies src of srclen to a, starting at a.v[start], growing a if needed using m.
void ArrayCopy(Array* nonull a, u32 start, const void* src, u32 srclen, Memory nullable m);

// Macros:
//   ArrayForEach(Array* nonull a, TYPE elemtype, NAME elemname, EXPR body)
//

// ------------------------------------------------------------------------------------------------
// inline implementations

inline static void ArrayInitWithStorage(Array* nonull a, void* nonull storagePtr, u32 storageSize){
  a->v = storagePtr;
  a->len = 0;
  a->cap = storageSize;
  a->onheap = false;
}

inline static void ArrayFree(Array* a, Memory mem) {
  if (a->onheap) {
    memfree(mem, a->v);

    #if DEBUG
    a->v = NULL;
    a->cap = 0;
    #endif
  }
}

inline static void ArrayPush(Array* a, void* v, Memory mem) {
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

// static void ArrayInit(Array* a) {
//   a->v = NULL;
//   a->cap = a->len = 0;
//   a->onheap = true;
// }

// Better to use Array_INIT
// Array_STACK_INIT(u32 capacity) => Array
// #define Array_STACK_INIT(capacity) (({ \
//   void* __ArrayStackStorage__##__LINE__[capacity]; \
//   Array a = { __ArrayStackStorage__##__LINE__, 0, (capacity), false }; \
//   a; \
// }))
