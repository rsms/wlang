#include "wp.h"
#include "memory.h"
#include "array.h"


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


// memsprintf is like sprintf but uses memory from mem.
char* memsprintf(Memory mem, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  size_t bufsize = (strlen(format) * 2) + 1;
  char* buf = memalloc(mem, bufsize);
  size_t idealsize = (size_t)vsnprintf(buf, bufsize, format, ap);
  if (idealsize >= bufsize) {
    // buf is too small
    buf = mspace_realloc(mem, buf, idealsize + 1);
    idealsize = (size_t)vsnprintf(buf, bufsize, format, ap);
    assert(idealsize < bufsize); // according to libc docs, this should be true
  }
  va_end(ap);
  return buf;
}


typedef struct GC {
  Array gen1, gen2;
} GC;


/*__thread*/ Memory _gmem = NULL;
/*__thread*/ GC tlsGC = { Array_INIT, Array_INIT };


Memory _GlobalMemory() {
  return (_gmem == NULL) ? (_gmem = create_mspace(0, 0)) : _gmem;
}


void* memgcalloc(size_t size) {
  void* ptr = mspace_calloc(_GlobalMemory(), 1, size);
  _memgc(ptr);
  return ptr;
}


void memgc_collect() {
  auto gc = &tlsGC;
  // dlog("memgc_collect gen1 %u, gen2 %u", gc->gen1.len, gc->gen2.len);

  // free anything in gen2
  if (gc->gen2.len > 0) {
    assert(_gmem != NULL);

    // Node: dlmalloc mentions that for large bulk_free sets, sorting the pointers first may
    // increases locality and may increase performance. If we ever decide to performance tune
    // this code, it may be worth considering.

    #if DEBUG
    size_t unfreed =
    #endif
    mspace_bulk_free(_gmem, gc->gen2.v, gc->gen2.len);

    #if DEBUG
    // unfreed is always zero in release builds as dlmalloc footers are only enabled in DEBUG.
    if (unfreed > 0) {
      dlog("[gc] warning: collector found %zu elements from a non-global allocator", unfreed);
    }
    #endif

    gc->gen2.len = 0;
  }

  // swap gen1 with gen2
  auto tmp = gc->gen2;
  gc->gen2 = gc->gen1;
  gc->gen1 = tmp;
}


// memgcmark marks ptr for garbage collection
void _memgc(void* ptr) {
  assert(_gmem != NULL); // ptr is allocated in the global allocator, so this should not be null
  auto gc = &tlsGC;
  ArrayPush(&gc->gen1, ptr, _gmem);
}


#if DEBUG
static void test() {
  // printf("-------------------------Memory-------------------------\n");

  u32 allocCount1 = 5;

  for (u32 i = 0; i < allocCount1; i++) {
    void* ptr = memalloc(NULL, 16);
    memgc(ptr);
  }

  auto gc = &tlsGC;

  assert(gc->gen1.len == allocCount1);

  memgc_collect();
  assert(gc->gen1.len == 0); // gen1 should always be 0 after call to memgc_collect

  assert(gc->gen2.len == allocCount1);

  u32 allocCount2 = 8;

  for (int i = 0; i < allocCount2; i++) {
    memgcalloc(16);
  }

  assert(gc->gen1.len == allocCount2);
  assert(gc->gen2.len == allocCount1);

  memgc_collect();
  assert(gc->gen1.len == 0);

  assert(gc->gen2.len == allocCount2);

  memgc_collect();

  assert(gc->gen1.len == 0);
  assert(gc->gen2.len == 0);

  // test bulk_free to ensure that we get an error message logged in case foreign pointers
  // are added to the GC.
  // Caution: This depend on FOOTER=1 being defined for dlmalloc, which is only the case
  // for DEBUG builds.
  {
    Memory mem = MemoryNew(0);
    memgc(memalloc(mem, 16));    // add pointer from unrelated mspace to gc
    memgc(memalloc(_gmem, 16)); // add pointer from the correct mspace to gc
    assert(gc->gen1.len == 2);
    size_t unfreed = mspace_bulk_free(_gmem, gc->gen1.v, gc->gen1.len);
    assert(unfreed == 1);
    MemoryFree(mem);
  }

  // printf("------------------------/Memory-------------------------\n");
  // exit(0);
}
W_UNIT_TEST(Memory, { test(); }) // W_UNIT_TEST
#endif
