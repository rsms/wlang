// AST node allocator
#include "wp.h"


// size of a bloc. Initialized from os_mempagesize() in NodeAllocatorInit.
static size_t blockSize = 0;


static inline NAllocBlock* allocBlock() {
  auto b = (NAllocBlock*)malloc(sizeof(NAllocBlock) + blockSize);
  b->offs = 0;
  return b;
}


// initialize new or reset existing for reuse.
void NodeAllocatorInit(NodeAllocator* na) {
  if (blockSize == 0) {
    blockSize = os_mempagesize();
  }
  // moves all blocks from mused to mfree and resets the blocks' offs.
  if (na->mfree == NULL) {
    // initialize new
    na->mfree = allocBlock();
    na->mfree->next = NULL;
  } else {
    dlog("TODO: NodeAllocatorInit recycle"); // TODO
  }
}


// free all its node memory
void NodeAllocatorFree(NodeAllocator* na) {
  // frees all blocks (mfree and mused lists) are freed.
  dlog("TODO: NodeAllocatorFree"); // TODO
}


// allocate a node from an allocator
Node* NAlloc(NodeAllocator* na, NodeKind kind) {
  // allocates space for a Node in the head block of the mfree list.
  auto b = na->mfree;

  // check if there is enough space in block b for a node
  if (blockSize - b->offs < sizeof(Node)) {
    auto usedb = b;
    b = b->next;
    if (b == NULL) {
      // No more free blocks. Allocate a new block.
      dlog("[NAlloc] new block");
      b = allocBlock();
    } else {
      dlog("[NAlloc] recycle block");
    }

    // set free block
    na->mfree = b;

    // add used block to used list
    usedb->next = na->mused;
    na->mused = usedb;
  }

  // allocate space in block
  Node* n = (Node*)&b->data[b->offs];
  b->offs += sizeof(Node);
  n->kind = kind;
  return n;
}

