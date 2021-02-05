#pragma once
#include "defs.h"
#include "memory.h"

// very simple array type
typedef struct {
  void** v;
  u32    cap;
  u32    len;
  bool   onheap;  // false if v is space on stack
} Array;

#define Array_INIT { NULL, 0, 0, true }

static void  ArrayInit(Array* nonull a);
static void  ArrayInitWithStorage(Array* nonull a, void* nonull storage, u32 storagecap);
static void  ArrayFree(Array* nonull a, Memory nullable mem);
void         ArrayGrow(Array* nonull a, size_t addl, nullable Memory mem); // cap=align2(len+addl)
static void  ArrayPush(Array* nonull a, void* nullable v, Memory nullable mem);
static void* ArrayPop(Array* nonull a);
void         ArrayRemove(Array* nonull a, u32 start, u32 count);
int          ArrayIndexOf(Array* nonull a, void* nullable entry); // -1 on failure

// ArrayCopy copies src of srclen to a, starting at a.v[start], growing a if needed using m.
void ArrayCopy(Array* nonull a, u32 start, const void* src, u32 srclen, Memory nullable m);

// The comparison function must return an integer less than, equal to, or greater than zero if
// the first argument is considered to be respectively less than, equal to, or greater than the
// second.
typedef int (ArraySortFun)(const void* elem1, const void* elem2, void* userdata);

// ArraySort sorts the array in place using comparator to rank entries
void ArraySort(Array* a, ArraySortFun* comparator, void* userdata);

// Macros:
//   ArrayForEach(Array* nonull a, TYPE elemtype, NAME elemname) <body>
//

// ------------------------------------------------------------------------------------------------
// inline implementations

inline static void ArrayInit(Array* nonull a) {
  a->v = 0;
  a->cap = 0;
  a->len = 0;
  a->onheap = true;
}

inline static void ArrayInitWithStorage(Array* nonull a, void* nonull ptr, u32 cap){
  a->v = ptr;
  a->cap = cap;
  a->len = 0;
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

#define ArrayForEach(a, ELEMTYPE, LOCALNAME)        \
  /* this for introduces LOCALNAME */               \
  for (auto LOCALNAME = (ELEMTYPE*)(a)->v[0];       \
       LOCALNAME == (ELEMTYPE*)(a)->v[0];           \
       LOCALNAME++)                                 \
  /* actual for loop */                             \
  for (                                             \
    u32 LOCALNAME##__i = 0,                         \
        LOCALNAME##__end = (a)->len;                \
    LOCALNAME = (ELEMTYPE*)(a)->v[LOCALNAME##__i],  \
    LOCALNAME##__i < LOCALNAME##__end;              \
    LOCALNAME##__i++                                \
  ) /* <body should follow here> */


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


// #define ArrayForEach1(a, ELEMTYPE, ELEMNAME, body)                                        \
//   do { for (u32 __i_ArrayForEach = 0; __i_ArrayForEach < (a)->len; __i_ArrayForEach++) { \
//     ELEMTYPE* ELEMNAME = (ELEMTYPE*)(a)->v[__i_ArrayForEach];                            \
//     { body }                                                                             \
//   } } while(0)
