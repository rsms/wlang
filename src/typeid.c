#include "wp.h"
#include "hash.h"

// See docs/typeid.md



// u64 hashFNV1a64(const u8* buf, size_t len)

// // allocate a node from an allocator
// static inline Node* NewNode(FWAllocator* na, NodeKind kind) {
//   Node* n = (Node*)FWAlloc(na, sizeof(Node));
//   n->kind = kind;
//   return n;
// }

static bool TypeEquals(const Node* t1, const Node* t2) {
  return t1 == t2;
}


W_UNIT_TEST(TypeID, {
  FWAllocator mem = {0};
  FWAllocInit(&mem);
  #define mknode(t) NewNode(&mem, (t))

  auto n = mknode(NType);

  dlog("hihih");
}) // W_UNIT_TEST
