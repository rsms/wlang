#include "wp.h"
#include "fwalloc.h"

// Forward-rewind slab allocator.
//
// See header file for a description of use cases and API.
//
// IMPLEMENTATION NOTES
//
// The allocator makes use of blocks (FWAllocBlock) which is a big chunk of memory.
// When a block runs out of space, a new block is allocated from the system and made the
// head (mfree) of the linked list. The used-up block is moved to the mused linked-list.
//
// FWAllocInit either:
//   if used, moves all blocks from mused to mfree and resets the blocks' offs.
//   else allocates one new block on mfree.
//
// FWAllocFree frees all blocks and in effect all memory.
//


// Size of a FWAllocBlock. Initialized from os_mempagesize() in FWAllocInit.
static size_t blockSize = 0;
size_t FWBlockCap = 0; // capacity for data ( = blockSize - sizeof(FWAllocBlock))


static inline FWAllocBlock* allocBlock() {
  auto b = (FWAllocBlock*)malloc(blockSize);
  memset(b, 0, blockSize);
  return b;
}


static inline void zeroBlock(FWAllocBlock* b) {
  // caution: struct-field order dependend code:
  // zero everything in block after the next pointer (don't touch next pointer) up until
  // b.offs which is the offset into b where zero memory starts.
  if (b->offs > 0) {
    void* start = ((u8*)b) + sizeof(void*);
    const size_t headerSize = sizeof(FWAllocBlock) - sizeof(void*);
    memset(start, 0, headerSize + b->offs);
  }
}


void FWAllocRecycle(FWAllocator* na) {
  assert(blockSize > 0);
  assert(na->mfree != NULL); // assumed to be initialized

  // recycles used and free blocks by moving all used blocks to the free list
  // and zeroing used memory.
  //
  // e.g. before recycle:
  //   mfree: block3
  //   mused: block2 -> block1 -> NULL
  // after recycle:
  //   mfree: block3 -> block2 -> block1 -> NULL
  //   mused: NULL
  //
  // note: it may appear as all we need to do is to set na->mfree->next = na->mused,
  // but there's a scenario where we recycle a previously-recycled allocator which
  // has not used up all its free blocks. Therefore we need to find the tail first.
  // As an optimization, we zero the first free block only, since Nth free block is
  // already zero.
  auto freetail = na->mfree;
  zeroBlock(freetail);  // reset current partially-used free block
  while (freetail->next != NULL) {
    // Note: Nth free block is already zero, so no need to call zeroBlock()
    freetail = freetail->next;
  }
  auto b = na->mused;
  while (b != NULL) {
    zeroBlock(b);
    freetail->next = b;
    freetail = b;
    b = b->next;
  }
  na->mused = NULL;
}


// initialize new or reset existing for reuse.
void FWAllocInit(FWAllocator* na) {
  if (blockSize == 0) {
    blockSize = os_mempagesize();
    FWBlockCap = blockSize - sizeof(FWAllocBlock);
  }
  if (na->mfree == NULL) {
    // initialize new
    na->mfree = allocBlock();
    na->mfree->next = NULL;
  } else {
    FWAllocRecycle(na);
  }
}


// free all block memory
void FWAllocFree(FWAllocator* na) {
  // frees all blocks (mfree and mused lists) are freed.
  assert(
    // either free and used lists are null, OR free list is not null.
    // it should not be possible to have a null free list and a non-null used list.
    (na->mfree == NULL && na->mused == NULL) ||
    na->mfree != NULL
  );
  auto b = na->mfree;
  while (b != NULL) {
    auto next = b->next;
    free(b);
    if (next == NULL) {
      // continue freeing used list
      b = na->mused;
      na->mused = NULL;
    } else {
      b = next;
    }
  }
  na->mfree = NULL;
}


void _FWAllocGrow(FWAllocator* na) {
  auto usedb = na->mfree;

  // use next free block or allocate a new block
  if ((na->mfree = na->mfree->next) == NULL) {
    na->mfree = allocBlock();
  }

  // move used block to used list
  usedb->next = na->mused;
  na->mused = usedb;
}


W_UNIT_TEST(fwalloc, {
  FWAllocator a;
  assert(a.mfree == NULL);
  assert(a.mused == NULL);

  FWAllocInit(&a);
  assert(blockSize > 0);
  assert(a.mfree != NULL);
  assert(a.mfree->next == NULL);
  assert(a.mfree->offs == 0);
  assert(a.mused == NULL);

  // test setup constants
  const size_t allocSize = 64; // bytes for each test allocation
  const size_t nodeCapacity = blockSize / allocSize;

  auto block1 = a.mfree;

  // allocate enough nodes to cause a new block to be allocated
  for (size_t i = 0; i < nodeCapacity + 1; i++) {
    assert(FWAlloc(&a, allocSize) != NULL);
  }
  assert(a.mfree != NULL);
  assert(a.mfree != block1);     // should be new block
  assert(a.mfree->next == NULL); // should be just one free block
  assert(a.mused == block1);     // should be one used block (block1)
  assert(a.mused->next == NULL); // should be one used block (block1)

  auto block2 = a.mfree;

  // cause another block to be allocated
  for (int i = 0; i < nodeCapacity + 1; i++) {
    assert(FWAlloc(&a, allocSize) != NULL);
  }
  assert(a.mfree != NULL);
  assert(a.mfree != block2);     // should be new block
  assert(a.mfree != block1);
  assert(a.mfree->next == NULL); // should be just one free block
  assert(a.mused == block2);     // should have two used blocks
  assert(a.mused->next == block1);
  assert(a.mused->next->next == NULL); // block1 is the last used block

  auto block3 = a.mfree;
  // dlog("block1 %p", block1);
  // dlog("block2 %p", block2);
  // dlog("block3 %p", block3);

  // recycle allocator
  FWAllocRecycle(&a);
  assert(a.mused == NULL); // used list should be empty
  // should have block 3-1 in free list:
  assert(a.mfree == block3);
  assert(a.mfree->next == block2);
  assert(a.mfree->next->next == block1);
  assert(a.mfree->next->next->next == NULL);
  // offset of each block should be 0
  auto b = a.mfree;
  while (b != NULL) {
    assert(b->offs == 0);
    // all block memory should be zero
    auto ptr = &b->data[0];
    auto end = ((u8*)ptr) + (blockSize - sizeof(FWAllocBlock));
    for (; ptr < end; ptr++) {
      assert(*ptr == 0);
    }
    b = b->next;
  }

  // allocate just a few nodes from the recycled allocator.
  // this should not cause any free blocks to be moved to the used list.
  for (int i = 0; i < nodeCapacity - 1; i++) {
    assert(FWAlloc(&a, allocSize) != NULL);
  }
  assert(a.mused == NULL); // used list should be empty
  assert(a.mfree == block3); // should be same, recycled blocks
  assert(a.mfree->next == block2);
  assert(a.mfree->next->next == block1);
  assert(a.mfree->next->next->next == NULL);

  // Recycle again
  FWAllocRecycle(&a);
  assert(a.mused == NULL); // used list should be empty
  assert(a.mfree == block3); // should be same, recycled blocks
  assert(a.mfree->next == block2);
  assert(a.mfree->next->next == block1);
  assert(a.mfree->next->next->next == NULL);

  // allocate enough memory to cause a free block to be moved to the used list
  for (int i = 0; i < nodeCapacity + 1; i++) {
    assert(FWAlloc(&a, allocSize) != NULL);
  }
  assert(a.mused == block3); // block3 should be used
  assert(a.mused->next == NULL);
  assert(a.mfree == block2); // block2 should be the current free block
  assert(a.mfree->next == block1);
  assert(a.mfree->next->next == NULL);

  // free memory
  FWAllocFree(&a);
  assert(a.mfree == NULL);
  assert(a.mused == NULL);
})

