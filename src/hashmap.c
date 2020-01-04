// example:
// #define HASHMAP_NAME     FooMap
// #define HASHMAP_KEY      Foo
// #define HASHMAP_KEY_HASH FooHash  // should return an unsigned integer
// #define HASHMAP_VALUE    char*
#ifndef HASHMAP_NAME
#error "please define HASHMAP_NAME"
#endif
#ifndef HASHMAP_KEY
#error "please define HASHMAP_KEY"
#endif
#ifndef HASHMAP_KEY_HASH
#error "please define HASHMAP_KEY_HASH"
#endif
#ifndef HASHMAP_VALUE
#error "please define HASHMAP_VALUE"
#endif

#define _HM_MAKE_FN_NAME(a, b) a ## b
#define _HM_FUN(prefix, name) _HM_MAKE_FN_NAME(prefix, name)
#define HM_FUN(name) _HM_FUN(HASHMAP_NAME, name)

static const size_t bucketSize = 6; // entries per bucket

typedef struct {
  struct {
    HASHMAP_KEY   key;
    HASHMAP_VALUE value;
  } entries[bucketSize];
} Bucket;

void HM_FUN(Init)(HASHMAP_NAME* m, size_t initbuckets) {
  m->cap = initbuckets;
  m->len = 0;
  m->buckets = calloc(m->cap, sizeof(Bucket));
}

void HM_FUN(Free)(HASHMAP_NAME* m) {
  free(m->buckets);
  #if DEBUG
  m->buckets = NULL;
  m->len = 0;
  m->cap = 0;
  #endif
}

static void mapGrow(HASHMAP_NAME* m) {
  size_t cap = m->cap * 2;
  rehash: {
    auto newbuckets = (Bucket*)calloc(cap, sizeof(Bucket));
    for (size_t bi = 0; bi < m->cap; bi++) {
      auto b = &((Bucket*)m->buckets)[bi];
      for (size_t i = 0; i < bucketSize; i++) {
        auto e = &b->entries[i];
        if (e->key == NULL) {
          break;
        }
        if (e->value == NULL) {
          // skip deleted entry (compactation)
          continue;
        }
        size_t index = ((size_t)HASHMAP_KEY_HASH(e->key)) % cap;
        auto newb = &newbuckets[index];
        bool fit = false;
        for (size_t i2 = 0; i2 < bucketSize; i2++) {
          auto e2 = &newb->entries[i2];
          if (e2->key == NULL) {
            // found a free slot in newb
            *e2 = *e;
            fit = true;
            break;
          }
        }
        if (!fit) {
          // no free slot found in newb; need to grow further.
          free(newbuckets);
          cap = cap * 2;
          goto rehash;
        }
      }
    }
    free(m->buckets);
    m->buckets = newbuckets;
    m->cap = cap;
  }
}


// HM_FUN(Set) inserts key=value into m.
// Returns replaced value or NULL if key did not exist in map.
HASHMAP_VALUE HM_FUN(Set)(HASHMAP_NAME* m, HASHMAP_KEY key, HASHMAP_VALUE value) {
  assert(value != NULL);
  while (1) { // grow loop
    size_t index = ((size_t)HASHMAP_KEY_HASH(key)) % m->cap;
    auto b = &((Bucket*)m->buckets)[index];
    // dlog("bucket(key=\"%s\") #%u  b=%p e=%p", key, index, b, &b->entries[0]);
    for (size_t i = 0; i < bucketSize; i++) {
      auto e = &b->entries[i];
      if (e->value == NULL) {
        // free slot
        e->key = key;
        e->value = value;
        m->len++;
        return NULL;
      }
      if (e->key == key) {
        // key already in map -- replace value
        auto oldval = e->value;
        e->value = value;
        return oldval;
      }
      // dlog("collision key=\"%s\" <> e->key=\"%s\"", key, e->key);
    }
    // overloaded -- grow buckets
    // dlog("grow & rehash");
    mapGrow(m);
  }
}


HASHMAP_VALUE HM_FUN(Del)(HASHMAP_NAME* m, HASHMAP_KEY key) {
  size_t index = ((size_t)HASHMAP_KEY_HASH(key)) % m->cap;
  auto b = &((Bucket*)m->buckets)[index];
  for (size_t i = 0; i < bucketSize; i++) {
    auto e = &b->entries[i];
    if (e->key == key) {
      if (!e->value) {
        break;
      }
      // mark as deleted
      auto value = e->value;
      e->value = NULL;
      m->len--;
      return value;
    }
  }
  return NULL;
}


HASHMAP_VALUE HM_FUN(Get)(const HASHMAP_NAME* m, HASHMAP_KEY key) {
  size_t index = ((size_t)HASHMAP_KEY_HASH(key)) % m->cap;
  auto b = &((Bucket*)m->buckets)[index];
  for (size_t i = 0; i < bucketSize; i++) {
    auto e = &b->entries[i];
    if (e->key == key) {
      return e->value;
    }
    if (e->key == NULL) {
      break;
    }
  }
  return NULL;
}


void HM_FUN(Clear)(HASHMAP_NAME* m) {
  memset(m->buckets, 0, sizeof(Bucket) * m->cap);
  m->len = 0;
}


void HM_FUN(Iter)(const HASHMAP_NAME* m, HM_FUN(Iterator)* it, void* userdata) {
  bool stop = false;
  for (size_t bi = 0; bi < m->cap; bi++) {
    auto b = &((Bucket*)m->buckets)[bi];
    for (size_t i = 0; i < bucketSize; i++) {
      auto e = &b->entries[i];
      if (e->key == NULL) {
        break;
      }
      if (e->value != NULL) {
        it(e->key, e->value, &stop, userdata);
        if (stop) {
          return;
        }
      }
    }
  }
}

static u32* hashmapDebugDistr(const HASHMAP_NAME* m) {
  size_t valindex = 0;
  u32* vals = (u32*)calloc(m->cap, sizeof(u32));
  for (size_t bi = 0; bi < m->cap; bi++) {
    auto b = &((Bucket*)m->buckets)[bi];
    u32 depth = 0;
    for (size_t i = 0; i < bucketSize; i++) {
      auto e = &b->entries[i];
      if (e->key == NULL) {
        break;
      }
      if (e->value != NULL) {
        depth++;
      }
    }
    vals[valindex++] = depth;
  }
  return vals;
}

#undef _HM_MAKE_FN_NAME
#undef _HM_FUN
#undef HM_FUN
