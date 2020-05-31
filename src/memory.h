#pragma once
#include "dlmalloc.h"

// Memory is an isolated-space memory allocator, useful for allocating many small
// short-lived fragments of memory, like for example AST nodes.
//
// Passing NULL to mangagement functions like memalloc uses a shared global allocator
// and works the same way as libc malloc, free et al.
//
typedef mspace Memory;

// memalloc allocates memory. If m=NULL, the global allocator is used.
// Returned memory is zeroed.
static void* memalloc(Memory mem, size_t size);

// memalloc reallocates some memory. Additional memory is NOT zeroed.
static void* memrealloc(Memory mem, void* ptr, size_t newsize);

// memfree frees memory, if ptr is at the tail, else does nothing (leaves hole.)
static void memfree(Memory mem, void* ptr);

// memallocCStr is like strdup
char* memallocCStr(Memory mem, const char* pch, size_t len);

// memallocCStrConcat concatenates up to 20 c-strings together.
// Arguments must be terminated with NULL.
char* memallocCStrConcat(Memory mem, const char* s1, ...);

// Returns a thread-local, program-global allocator.
Memory GlobalMemory();

// Create a new memory space
Memory MemoryNew(size_t initHint/*=0*/);
void MemoryRecycle(Memory* memptr); // recycle for reuse
void MemoryFree(Memory mem);        // free all memory allocated by mem

// -------------------------------------------------------------
// inline implementations

inline static void* memalloc(Memory mem, size_t size) {
  return mspace_calloc(mem == NULL ? GlobalMemory() : mem, 1, size);
}
inline static void* memrealloc(Memory mem, void* ptr, size_t newsize) {
  return mspace_realloc(mem == NULL ? GlobalMemory() : mem, ptr, newsize);
}
inline static void memfree(Memory mem, void* ptr) {
  mspace_free(mem == NULL ? GlobalMemory() : mem, ptr);
}
