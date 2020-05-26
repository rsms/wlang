#include "wp.h"
#include "fwalloc.h"

static _Thread_local FWAllocator tls_alloc = {0};


// ------------------------------------------------------------------------------------------
//
// Considering a ring buffer approach instead.
//
// The use for TmpData is limited to very short lifetimes, usually just within one
// call frame or even basic block.
//
// A lot of the use looks like this:
//   auto tmp1 = (char*)TmpData(10); // populate tmp1
//   auto tmp2 = (char*)TmpData(10); // populate tmp2
//   printf("values: %s, %s", tmp1, tmp2);
//   // tmp1 and tmp2 not used beyond this point
//
// A ring buffer approach has some inherent risk: if the sum of allocations in one "frame"
// is larger than the buffer, data is corrupted. One option is to use a token:
//
//   TmpDataToken tmp = 0;
//   auto tmp1 = (char*)TmpData(&tmp, 10); // populate tmp1
//   auto tmp2 = (char*)TmpData(&tmp, 10); // populate tmp2
//   printf("values: %s, %s", tmp1, tmp2);
//
// The token could simply be the allocation size. We can't just use a ending call since
// tmpdata is often returned from functions.
// A token aproach would work only if allocations happen with the same token.
// The following would work:
//
//   const char* fooName(const Foo f) {
//     TmpDataToken tmp = 0;
//     auto name = (char*)TmpData(&tmp, 10);
//     // populate name
//     return name;
//   }
//   printf("foo: %s", fooName(foo));
//
// However this would not work:
//
//   auto name1 = fooName(foo1);
//   auto name2 = fooName(foo2);  // might overwrite the data allocated for name1
//   printf("foos: %s, %s", name1, name2);
//
// So in conclusion, using a forward-rewind allocator instead is a more robust choice
// which may cause higher memory usage than some more elaborate alernative.
// A forward-rewind allocator is also really simple, and simple is good.
//
// ------------------------------------------------------------------------------------------

// Note: If we struggle to find a perfect place to call TmpRecycle(), for example if the
// driver code might have refs to tmpdata for an extra cycle, we could apply double-buffering
// here. I.e. TmpRecycle() would swap the active allocator with a secondary one.


// allocate size number of bytes to be used for a short amount of time.
void* TmpData(size_t size) {
  if (tls_alloc.mfree == NULL) {
    FWAllocInit(&tls_alloc);
  }
  return FWAlloc(&tls_alloc, size);
}

// frees all memory allocated with TmpData
void TmpRecycle() {
  if (tls_alloc.mfree != NULL) {
    FWAllocRecycle(&tls_alloc);
  }
}


W_UNIT_TEST(TmpData, {
  TmpRecycle();
  TmpRecycle(); // no crash

  auto p1 = (char*)TmpData(8);
  auto p2 = (char*)TmpData(8);
  auto p3 = (char*)TmpData(8);

  // each allocation is a new region of memory that is at least 8 bytes as requested.
  assert(p1 != p2);
  assert(p1 != p3);
  assert(p2 != p3);
  assert((uintptr_t)p2 < (uintptr_t)p1 || (uintptr_t)p2 >= ((uintptr_t)p1)+8);
  assert((uintptr_t)p3 < (uintptr_t)p2 || (uintptr_t)p3 >= ((uintptr_t)p2)+8);

  // memory should be zeroed
  assert(memcmp(p1, "\0\0\0\0\0\0\0\0", 8) == 0);
  assert(memcmp(p2, "\0\0\0\0\0\0\0\0", 8) == 0);
  assert(memcmp(p3, "\0\0\0\0\0\0\0\0", 8) == 0);

  // write to memory now so we can check zeroing after recycle
  *p1 = '1';
  *p2 = '2';
  *p3 = '3';
  assert(memcmp(p1, "1\0\0\0\0\0\0\0", 8) == 0);
  assert(memcmp(p2, "2\0\0\0\0\0\0\0", 8) == 0);
  assert(memcmp(p3, "3\0\0\0\0\0\0\0", 8) == 0);

  TmpRecycle();

  // memory should be zeroed from TmpRecycle call
  assert(memcmp(p1, "\0\0\0\0\0\0\0\0", 8) == 0);
  assert(memcmp(p2, "\0\0\0\0\0\0\0\0", 8) == 0);
  assert(memcmp(p3, "\0\0\0\0\0\0\0\0", 8) == 0);

}) // W_UNIT_TEST
