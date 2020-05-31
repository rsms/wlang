#include "wp.h"
#include "memory.h"


static size_t memPageSize = 0;

static void __attribute__((constructor)) init() {
  memPageSize = os_mempagesize();
}

Memory MemoryNew(size_t initHint) {
  if (initHint == 0) {
    initHint = memPageSize;
  }
  return create_mspace(/*capacity*/initHint, /*locked*/0);
}

void MemoryRecycle(Memory* memptr) {
  // TODO: see if there is a way to make dlmalloc reuse msp
  destroy_mspace(*memptr);
  *memptr = create_mspace(/*capacity*/memPageSize, /*locked*/0);
}

void MemoryFree(Memory mem) {
  destroy_mspace(mem);
}


char* memallocCStr(Memory mem, const char* pch, size_t len) {
  auto s = (char*)memalloc(mem, len + 1);
  memcpy(s, pch, len);
  s[len] = 0;
  return s;
}

char* memallocCStrConcat(Memory mem, const char* s1, ...) {
  va_list ap;

  size_t len1 = strlen(s1);
  size_t len = len1;
  u32 count = 0;
  va_start(ap, s1);
  while (1) {
    const char* s = va_arg(ap,const char*);
    if (s == NULL || count == 20) { // TODO: warn about limit somehow?
      break;
    }
    len += strlen(s);
  }
  va_end(ap);

  char* newstr = (char*)memalloc(mem, len + 1);
  char* dstptr = newstr;
  memcpy(dstptr, s1, len1);
  dstptr += len1;

  va_start(ap, s1);
  for (u32 i = 0; i < count; i++) {
    const char* s = va_arg(ap,const char*);
    auto len = strlen(s);
    memcpy(dstptr, s, len);
    dstptr += len;
  }
  va_end(ap);

  *dstptr = 0;

  return newstr;
}


// static _Thread_local Memory tlsMem = NULL;
static __thread Memory tlsMem = NULL;

Memory GlobalMemory() {
  if (tlsMem == NULL) {
    tlsMem = create_mspace(0, 0);
  }
  return tlsMem;
}

