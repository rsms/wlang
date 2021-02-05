#include "defs.h"
#include "memory.h"
#include "buf.h"

// Do not allocate more than this much extra memory in a call to _BufMakeRoomFor
#define BUF_MAX_PREALLOC (1024*1024)

void BufInit(Buf* b, Memory mem, size_t cap) {
  b->mem = mem;
  if (cap > 0) {
    b->ptr = (u8*)memalloc(mem, cap);
  } else {
    b->ptr = NULL;
  }
  b->cap = cap;
  b->len = 0;
}

void BufFree(Buf* b) {
  if (b->ptr != NULL) {
    memfree(b->mem, b->ptr);
  }
  #if DEBUG
  memset(b, 0, sizeof(Buf));
  #endif
}

void _BufMakeRoomFor(Buf* b, size_t size) {
  size_t cap = align2(b->len + size, 32);
  // Anticipate growing more; allocate some extra space beyond what is needed:
  if (cap < BUF_MAX_PREALLOC) {
    cap *= 2;
  } else {
    // Reached the limit of preallocation size.
    // Instead of doubling the allocating, add on a constant.
    cap += BUF_MAX_PREALLOC;
  }
  b->ptr = memrealloc(b->mem, b->ptr, cap);
  b->cap = cap;
}

// Adds a string to the string table. Returns the strtab offset.
void BufAppend(Buf* b, const void* ptr, size_t size) {
  BufMakeRoomFor(b, size);
  memcpy(&b->ptr[b->len], ptr, size);
  b->len += size;
}

u8* BufAlloc(Buf* b, size_t size) {
  BufMakeRoomFor(b, size);
  u8* ptr = &b->ptr[b->len];
  b->len += size;
  return ptr;
}

u8* BufAllocz(Buf* b, size_t size) {
  BufMakeRoomFor(b, size);
  u8* ptr = &b->ptr[b->len];
  memset(&b->ptr[b->len], 0, size);
  b->len += size;
  return ptr;
}

void BufAppendFill(Buf* b, u8 v, size_t size) {
  BufMakeRoomFor(b, size);
  memset(&b->ptr[b->len], v, size);
  b->len += size;
}