//
// Left-leaning red-black tree implementation.
// Based on the paper "Left-leaning Red-Black Trees" by Robert Sedgewick.
//
// This file is intended not to be compiled directly, but to be included by
// another C file. All functions are "static" and thus has hidden visibility.
//
// The following lists marcos and fucntions which are expected to be defined and
// implemented by the file that includes this file:

#ifndef RBKEY
  #error "please define RBKEY to your key type (e.g. const char*)"
#endif
// Note: define RBVALUE to have a "value" field added to nodes and functions
// supporting values (i.e. map/dictionary.) Activates RBGet in addition to RBHas.
#ifdef RBVALUE
  #ifndef RBVALUE_NOT_FOUND
    // Value returned when key lookup fails
    #define RBVALUE_NOT_FOUND ((RBVALUE)NULL)
  #endif
#endif

// The node type
typedef struct RBNode {
  RBKEY          key;
#ifdef RBVALUE
  RBVALUE        value;
#endif
  bool           isred;
  struct RBNode* left;
  struct RBNode* right;
} RBNode;

// Implement the following functions:

// RBAllocNode allocates memory for a node. It's called by RBSet.
static RBNode* RBAllocNode();

// RBFreeNode frees up a no-longer used node. Called by RBDelete and RBFree.
//   Invariant: RBNode->left == NULL
//   Invariant: RBNode->right == NULL
static void RBFreeNode(RBNode* node);

#ifdef RBVALUE
// RBFreeNode frees up a no-longer used value. Called by RBSet when replacing a value.
static void RBFreeValue(RBVALUE);
#endif

// RBCmp compares two keys. Return a negative value to indicate that the left
// key is smaller than the right, a positive value for the inverse and zero
// to indicate the keys are identical.
static int RBCmp(RBKEY a, RBKEY b);

// Example implementation:
// inline static RBNode* RBAllocNode() {
//   return (RBNode*)malloc(sizeof(RBNode));
// }
// inline static void RBFreeValue(RBVALUE v) {
//   BarFree((Bar*)v);
// }
// inline static void RBFreeNode(RBNode* node) {
//   FooFree((Foo*)node->key);
//   RBFreeValue(node->value);
//   free(node);
// }
// inline static int RBCmp(RBKEY a, RBKEY b) {
//   return a < b ? -1 : b < a ? 1 : 0;
// }

// -------------------------------------------------------------------------------------
// API implemented by the remainder of this file

// RBHas performs a lookup of k. Returns true if found.
static bool RBHas(const RBNode* n, RBKEY k);

#ifdef RBVALUE
  // RBGet performs a lookup of k. Returns value or RBVALUE_NOT_FOUND.
  static RBVALUE RBGet(const RBNode* n, RBKEY k);

  // RBSet adds or replaces value for k. Returns new n.
  static RBNode* RBSet(RBNode* n, RBKEY k, RBVALUE v);

  // RBSet adds value for k if it does not exist. Returns new n.
  // "added" is set to true when a new value was added, false otherwise.
  static RBNode* RBAdd(RBNode* n, RBKEY k, RBVALUE v, bool* added);
#else
  // RBInsert adds k. May modify tree even if k exists. Returns new n.
  static RBNode* RBInsert(RBNode* n, RBKEY k);
#endif

// RBDelete removes k if found. Returns new n.
static RBNode* RBDelete(RBNode* n, RBKEY k);

// RBClear removes all entries. n is invalid after this operation.
static void RBClear(RBNode* n);

// Iteration. Return true from callback to keep going.
typedef bool(RBIterator)(const RBNode* n, void* userdata);
static bool RBIter(const RBNode* n, RBIterator* f, void* userdata);

// RBCount returns the number of entries starting at n. O(n) time complexity.
static size_t RBCount(const RBNode* n);

// RBRepr formats n as printable lisp text, useful for inspecting a tree.
// keystr should produce a string representation of a given key.
static Str RBRepr(const RBNode* n, Str s, int depth, Str(keyfmt)(Str,RBKEY));

// -------------------------------------------------------------------------------------


static RBNode* rbNewNode(
  RBKEY key
#ifdef RBVALUE
, RBVALUE value
#endif
) {
  RBNode* node = RBAllocNode();
  if (node) {
    node->key   = key;
    #ifdef RBVALUE
    node->value = value;
    #endif
    node->isred = true;
    node->left  = NULL;
    node->right = NULL;
  } // else: out of memory
  return node;
}

static void flipColor(RBNode* node) {
  node->isred        = !node->isred;
  node->left->isred  = !node->left->isred;
  node->right->isred = !node->right->isred;
}

static RBNode* rotateLeft(RBNode* l) {
  auto r = l->right;
  l->right = r->left;
  r->left  = l;
  r->isred = l->isred;
  l->isred = true;
  return r;
}

static RBNode* rotateRight(RBNode* r) {
  auto l = r->left;
  r->left  = l->right;
  l->right = r;
  l->isred = r->isred;
  r->isred = true;
  return l;
}

// -------------------------------------------------------------------------------------

inline static void RBClear(RBNode* node) {
  assert(node);
  if (node->left) {
    RBClear(node->left);
  }
  if (node->right) {
    RBClear(node->right);
  }
  node->left = (RBNode*)0;
  node->right = (RBNode*)0;
  RBFreeNode(node);
}

// -------------------------------------------------------------------------------------


inline static bool RBHas(const RBNode* node, RBKEY key) {
  do {
    int cmp = RBCmp(key, (RBKEY)node->key);
    if (cmp == 0) {
      return true;
    }
    node = cmp < 0 ? node->left : node->right;
  } while (node);
  return false;
}


#ifdef RBVALUE
inline static RBVALUE RBGet(const RBNode* node, RBKEY key) {
  do {
    int cmp = RBCmp(key, (RBKEY)node->key);
    if (cmp == 0) {
      return (RBVALUE)node->value;
    }
    node = cmp < 0 ? node->left : node->right;
  } while (node);
  return RBVALUE_NOT_FOUND;
}
#endif


inline static size_t RBCount(const RBNode* n) {
  size_t count = 1;
  if (n->left) {
    count += RBCount(n->left);
  }
  return n->right ? count + RBCount(n->right) : count;
}

// -------------------------------------------------------------------------------------
// insert

// RB_TREE_VARIANT changes the tree type. See https://en.wikipedia.org/wiki/2–3–4_tree
// - #define RB_TREE_VARIANT 3:  2-3 tree
// - #define RB_TREE_VARIANT 4:  2-3-4 tree
#define RB_TREE_VARIANT 4


inline static bool isred(RBNode* node) {
  return node && node->isred;
}


static RBNode* rbInsert(
  RBNode* node,
  RBKEY key
#ifdef RBVALUE
, RBVALUE value
#endif
) {
  if (!node) {
    return rbNewNode(key
      #ifdef RBVALUE
      , value
      #endif
    );
  }

  #if RB_TREE_VARIANT != 3
  // build a 2-3-4 tree
  if (isred(node->left) && isred(node->right)) { flipColor(node); }
  #endif

  int cmp = RBCmp(key, node->key);

  #ifdef RBVALUE
  if (cmp == 0) {
    // exists
    auto oldval = node->value;
    node->value = value;
    RBFreeValue(oldval);
  } else if (cmp < 0) {
    node->left = rbInsert(node->left, key, value);
  } else {
    node->right = rbInsert(node->right, key, value);
  }
  #else
  if (cmp < 0) {
    node->left = rbInsert(node->left, key);
  } else if (cmp > 0) {
    node->right = rbInsert(node->right, key);
  } // else: key exists
  #endif

  if (isred(node->right) && !isred(node->left))     { node = rotateLeft(node); }
  if (isred(node->left) && isred(node->left->left)) { node = rotateRight(node); }

  #if RB_TREE_VARIANT == 3
  // build a 2-3 tree
  if (isred(node->left) && isred(node->right)) { flipColor(node); }
  #endif

  return node;
}



#ifdef RBVALUE

inline static RBNode* RBSet(RBNode* root, RBKEY key, RBVALUE value) {
  root = rbInsert(root, key, value);
  if (root) {
    // Note: rbInsert returns NULL when out of memory (malloc failure)
    root->isred = false;
  }
  return root;
}

inline static RBNode* RBAdd(RBNode* root, RBKEY key, RBVALUE value, bool* added) {
  if (!RBHas(root, key)) {
    *added = true;
    root = rbInsert(root, key, value);
    if (root) {
      root->isred = false;
    }
  } else {
    *added = false;
  }
  return root;
}

#else

inline static RBNode* RBInsert(RBNode* root, RBKEY key) {
  root = rbInsert(root, key);
  if (root) {
    // Note: rbInsert returns NULL when out of memory (malloc failure)
    root->isred = false;
  }
  return root;
}

#endif


// -------------------------------------------------------------------------------------
// delete


static RBNode* fixUp(RBNode* node) {
  // re-balance
  if (isred(node->right)) {
    node = rotateLeft(node);
  }
  if (isred(node->left) && isred(node->left->left)) {
    node = rotateRight(node);
  }
  if (isred(node->left) && isred(node->right)) {
    flipColor(node);
  }
  return node;
}

static RBNode* moveRedLeft(RBNode* node) {
  flipColor(node);
  if (isred(node->right->left)) {
    node->right = rotateRight(node->right);
    node = rotateLeft(node);
    flipColor(node);
  }
  return node;
}

static RBNode* moveRedRight(RBNode* node) {
  flipColor(node);
  if (isred(node->left->left)) {
    node = rotateRight(node);
    flipColor(node);
  }
  return node;
}

static RBNode* minNode(RBNode* node) {
  while (node->left) {
    node = node->left;
  }
  return node;
}

static RBNode* rbDeleteMin(RBNode* node) {
  if (!node->left) {
    // found; delete
    assert(node->right == NULL);
    RBFreeNode(node);
    return NULL;
  }
  if (!isred(node->left) && !isred(node->left->left)) {
    node = moveRedLeft(node);
  }
  node->left = rbDeleteMin(node->left);
  return fixUp(node);
}

static RBNode* rbDelete(RBNode* node, RBKEY key) {
  assert(node);
  int cmp = RBCmp(key, node->key);
  if (cmp < 0) {
    assert(node->left != NULL);
    if (!isred(node->left) && !isred(node->left->left)) {
      node = moveRedLeft(node);
    }
    node->left = rbDelete(node->left, key);
  } else {
    if (isred(node->left)) {
      node = rotateRight(node);
      cmp = RBCmp(key, node->key);
    }
    if (cmp == 0 && node->right == NULL) {
      // found; delete
      assert(node->left == NULL);
      RBFreeNode(node);
      return NULL;
    }
    assert(node->right != NULL);
    if (!isred(node->right) && !isred(node->right->left)) {
      node = moveRedRight(node);
      cmp = RBCmp(key, node->key);
    }
    if (cmp == 0) {
      assert(node->right);
      auto m = minNode(node->right);
      node->key = m->key;
      m->key = (RBKEY)NULL; // key moved; signal to RBFreeNode
      #ifdef RBVALUE
      node->value = m->value;
      #endif
      node->right = rbDeleteMin(node->right);
    } else {
      node->right = rbDelete(node->right, key);
    }
  }
  return fixUp(node);
}


inline static RBNode* RBDelete(RBNode* node, RBKEY key) {
  if (node) {
    node = rbDelete(node, key);
    if (node) {
      node->isred = false;
    }
  }
  return node;
}


// ---------------------------------------------------------------------------------------



inline static bool RBIter(const RBNode* n, RBIterator* f, void* userdata) {
  if (!f(n, userdata)) {
    return false;
  }
  if (n->left && !RBIter(n->left, f, userdata)) {
    return false;
  }
  if (n->right && !RBIter(n->right, f, userdata)) {
    return false;
  }
  return true;
}


inline static Str RBRepr(const RBNode* n, Str s, int depth, Str(keyfmt)(Str,RBKEY)) {
  if (depth > 0) {
    s = sdscatlen(s, "\n", 1);
    s = sdsgrow(s, sdslen(s) + depth, ' ');
  }
  s = sdscatlen(s, n->isred ? "(R " : "(B ", 3);
  s = keyfmt(s, n->key);
  if (n->left) {
    s = RBRepr(n->left, s, depth + 2, keyfmt);
  }
  if (n->right) {
    s = RBRepr(n->right, s, depth + 2, keyfmt);
  }
  s = sdscatlen(s, ")", 1);
  return s;
}


