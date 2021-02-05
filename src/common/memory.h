#pragma once
#include "defs.h"
#include "dlmalloc.h"
#include "sds.h"

// Memory is an isolated-space memory allocator, useful for allocating many small
// short-lived fragments of memory, like for example AST nodes.
//
// Passing NULL to mangagement functions like memalloc uses a shared global allocator
// and works the same way as libc malloc, free et al.
//
typedef mspace Memory;

// memalloc allocates memory. Returned memory is zeroed.
static void* memalloc(Memory nullable mem, size_t size) nonull_return;

// memalloct is a convenience for: (MyStructType*)memalloc(m, sizeof(MyStructType))
#define memalloct(mem, TYPE) ((TYPE*)memalloc(mem, sizeof(TYPE)))

// memalloc reallocates some memory. Additional memory is NOT zeroed.
static void* memrealloc(Memory nullable mem, void* nullable ptr, size_t newsize) nonull_return;

// memfree frees memory.
static void memfree(Memory nullable mem, void* nonull ptr);

// memallocCStr is like strdup
char* memallocCStr(Memory nullable mem, const char* nonull pch, size_t len);

// memallocCStrConcat concatenates up to 20 c-strings together.
// Arguments must be terminated with NULL.
char* memallocCStrConcat(Memory nullable mem, const char* nonull s1, ...);

// memsprintf is like sprintf but uses memory from mem.
char* memsprintf(Memory mem, const char* format, ...);

// -----------------------------------------------------------------------------------------------
// Rudimentary garbage collector for short-lived data.

// memgcalloc allocates memory that will be free'd automatically.
// This is equivalent to: memgc(memalloc(NULL, size))
void* memgcalloc(size_t size) nonull_return;

// memgcalloct is a convenience for: (MyStructType*)memgcalloc(sizeof(MyStructType))
#define memgcalloct(mem, TYPE) ((TYPE)*memgcalloc(mem, sizeof(TYPE)))

// memgc marks ptr for garbage collection.
// Memory which has been marked for garbage collection must not be freed manually.
// Note: If we ever have the need, add a memgc_remove function for explicitly removing a pointer.
// T memgc<T extends void*>(T ptr)
#define memgc(ptr) ({ _memgc(ptr); (ptr); })

// memgcsds marks an sds string for garbage collection. (Does not work with Sym.)
static sds memgcsds(sds nonull s) nonull_return;

// memgc_collect performs very basic garbage collection.
// Each Memory space maintains two lists for gc: gen1 and gen2. memgc(ptr) adds to gen1.
// When memgc_collect is called:
// 1. every pointer in gen2 is free'd; gen2 list is emptied.
// 2. every pointer in gen1 is moved to gen2.
// Thus, this is NOT a generic "smart" garbage collector.
// Caution: Calling memgc_collect twice in a row causes all gc objects to be free'd immediately.
// Always uses the global allocator.
void memgc_collect();


// -----------------------------------------------------------------------------------------------
// Memory spaces

// Create a new memory space
Memory MemoryNew(size_t initHint/*=0*/);
void MemoryRecycle(Memory* memptr); // recycle for reuse
void MemoryFree(Memory mem);        // free all memory allocated by mem

// -----------------------------------------------------------------------------------------------
// inline and internal implementations

void _memgc(void* nonull ptr);
Memory _GlobalMemory() nonull_return;

inline static void* memalloc(Memory mem, size_t size) {
  return mspace_calloc(mem == NULL ? _GlobalMemory() : mem, 1, size);
}

inline static void* memrealloc(Memory mem, void* ptr, size_t newsize) {
  return mspace_realloc(mem == NULL ? _GlobalMemory() : mem, ptr, newsize);
}

inline static void memfree(Memory mem, void* ptr) {
  mspace_free(mem == NULL ? _GlobalMemory() : mem, ptr);
}

inline static sds memgcsds(sds s) {
  _memgc( ((char*)s) - sdsHdrSize(s[-1]) );
  return s;
}
