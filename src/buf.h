#pragma once

typedef struct Buf {
  Memory mem;
  u8*    ptr;
  u64    cap;
  u64    len;
} Buf;

void BufInit(Buf* nonull b, Memory nullable mem, u64 cap);
void BufFree(Buf* nonull b);
void BufMakeRoomFor(Buf* nonull b, u64 size); // ensures there is capacity for at least size
void BufAppend(Buf* nonull b, const void* nonull ptr, u64 size);
void BufAppendFill(Buf* nonull b, u8 v, u64 count); // append count bytes of value v
u8*  BufAlloc(Buf* nonull b, u64 size); // like BufAppend but leaves allocated data untouched.
