#pragma once

typedef struct Buf {
  Memory mem;
  u8*    ptr;
  size_t cap;
  size_t len;
} Buf;

void BufInit(Buf* nonull b, Memory nullable mem, size_t cap);
void BufFree(Buf* nonull b);
void BufMakeRoomFor(Buf* nonull b, size_t size); // ensures there is capacity for at least size
void BufAppend(Buf* nonull b, const void* nonull ptr, size_t size);
void BufAppendFill(Buf* nonull b, u8 v, size_t count); // append count bytes of value v
u8*  BufAlloc(Buf* nonull b, size_t size); // like BufAppend but leaves allocated data untouched.

inline static void BufAppendc(Buf* b, char c) {
  if (b->cap <= b->len) { BufMakeRoomFor(b, 1); }
  b->ptr[b->len++] = (u8)c;
}
