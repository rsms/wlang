#pragma once

// FWAllocator is a forward-rewind allocator, useful for allocating many small
// short-lived fragments of memory, like for example AST nodes.
//
// Important: the size of a single allocation is limited by FWBlockCap.
// This means that fwalloc is NOT suitable for memory that might grow unbounded
// like for instance an array.
//
// The way this works is that items are allocated as needed, but freed in one
// big sweep (rewind), when allocated items are no longer needed, rather than
// items beeing freeded as soon as they are not used.
//
// This simplifies code like AST constructor where tracking use is either
// costly (ref counting) or very hard (cyclic ownership.)
//
// An allocator can be thought of as one big bag of items which is manually
// garbage collected (by calling FWAllocFree.)
//
typedef struct FWAllocBlock FWAllocBlock;
typedef struct FWAllocBlock {
  // if you change field order, you need to update impl of FWAllocRecycle
  FWAllocBlock* next;   // linked list
  size_t        offs;   // offset in data to free memory
  u8            data[]; // start of memory
} FWAllocBlock;
typedef struct {
  FWAllocBlock* mfree; // free memory linked-list head.
  FWAllocBlock* mused; // used memory linked-list head.
} FWAllocator;

size_t FWBlockCap;

void FWAllocInit(FWAllocator*);     // initialize new or recycle for reuse
void FWAllocRecycle(FWAllocator*);  // recycle for reuse
void FWAllocFree(FWAllocator*);     // free all memory allocated by an allocator
void _FWAllocGrow(FWAllocator*);    // internal function used by FWAlloc

// TODO: void* FWRealloc(FWAllocator* na, void* ptr, size_t size);

// FWAlloc allocates some memory
static inline void* FWAlloc(FWAllocator* na, size_t size) {
  // allocates space for a Node in the head block of the mfree list.
  if (FWBlockCap - na->mfree->offs < size) {
    assert(size <= FWBlockCap);
    _FWAllocGrow(na);
  }
  // allocate space in current free block
  auto b = na->mfree;
  void* ptr = (void*)&b->data[b->offs];
  b->offs += size;
  return ptr;
}
