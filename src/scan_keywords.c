typedef struct BTreeNode BTreeNode;
typedef struct BTreeNode {
  u32             keylen;
  const u8*       key;
  Tok             tok;
  const BTreeNode *left;
  const BTreeNode *right;
} BTreeNode;

#include "scan_keywords_def.c"

const BTreeNode* bt_lookup(const u8* key, u32 keylen, const BTreeNode* n) {
  while (n) {
    auto c = memcmp(key, n->key, min(keylen, n->keylen));
    // dlog("memcmp '%.*s' <> '%.*s' => %d",
    //   (int)keylen, key,
    //   (int)n->keylen, n->key,
    //   c);
    if (c < 0) {
      n = n->left;
    } else if (c > 0) {
      n = n->right;
    } else if (keylen == n->keylen) {
      return n;
    } else {
      break;
    }
  }
  return NULL;
}

Tok kwlookup(const u8* ptr, u32 len) {
  // dlog("kwlookup '%.*s'", (int)len, ptr);
  if (len >= KEYWORD_MINLEN && len <= KEYWORD_MAXLEN) {
    const BTreeNode* n = bt_lookup(ptr, len, bn_root);
    if (n) {
      // dlog("kwlookup => %s", TokName(n->tok));
      return n->tok;
    }
  }
  return TName;
}
