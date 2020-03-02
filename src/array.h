#pragma once

// very simple array type
typedef struct {
  void** v;
  u32    len;
  u32    cap;
  bool   onheap;  // false if v is space on stack
} Array;

#define Array_INIT { NULL, 0, 0, true }

#define Array_STACK_INIT(cap) (({ \
  void* __ArrayStackStorage__##__LINE__[cap]; \
  Array a = { __ArrayStackStorage__##__LINE__, 0, (cap), false }; \
  a; \
}))

static void ArrayInit(Array* a) {
  a->v = NULL;
  a->cap = a->len = 0;
  a->onheap = true;
}

static void ArrayFree(Array* a) {
  if (a->onheap) {
    free(a->v);
    a->v = NULL;
    a->cap = 0;
  }
}

// cap = align2(len + addl)
void ArrayGrow(Array*, size_t addl);

inline static void ArrayPush(Array* a, void* v) {
  if (a->len == a->cap) {
    ArrayGrow(a, 1);
  }
  a->v[a->len++] = v;
}

inline static void* ArrayPop(Array* a) {
  return a->len > 0 ? a->v[--a->len] : NULL;
}
