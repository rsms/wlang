// example:
// #define HASHMAP_NAME     FooMap
// #define HASHMAP_KEY      Foo
// #define HASHMAP_VALUE    char*
#ifndef HASHMAP_NAME
#error "please define HASHMAP_NAME"
#endif
#ifndef HASHMAP_KEY
#error "please define HASHMAP_KEY"
#endif
#ifndef HASHMAP_VALUE
#error "please define HASHMAP_VALUE"
#endif

#define _HM_MAKE_FN_NAME(a, b) a ## b
#define _HM_FUN(prefix, name) _HM_MAKE_FN_NAME(prefix, name)
#define HM_FUN(name) _HM_FUN(HASHMAP_NAME, name)
#define HASHMAP_IS_INIT(m) ((m)->buckets != NULL)


typedef struct {
  size_t cap;  // number of buckets
  size_t len;  // number of key-value entries
  void*  buckets; // internal
} HASHMAP_NAME;

#ifdef HASHMAP_INCLUDE_DECLARATIONS
// Include declarations.
// Normally these are copy-pasted and hand-converted in the user-level header.

// Init initializes a map structure. initbuckets is the number of initial buckets.
void HM_FUN(Init)(HASHMAP_NAME*, size_t initbuckets);

// Free frees buckets data.
void HM_FUN(Free)(HASHMAP_NAME*);

// Get searches for key. Returns value, or NULL if not found.
HASHMAP_VALUE HM_FUN(Get)(const HASHMAP_NAME*, Sym key);

// Set inserts key=value into m. Returns the replaced value or NULL if not found.
HASHMAP_VALUE HM_FUN(Set)(HASHMAP_NAME*, Sym key, HASHMAP_VALUE value);

// Del removes value for key. Returns the removed value or NULL if not found.
HASHMAP_VALUE HM_FUN(Del)(HASHMAP_NAME*, Sym key);

// Clear removes all entries. In contrast to Free, map remains valid.
void HM_FUN(Clear)(HASHMAP_NAME*);

// Iterator function type. Set stop=true to stop iteration.
typedef void(HM_FUN(Iterator))(Sym key, HASHMAP_VALUE value, bool* stop, void* userdata);

// Iter iterates over entries of the map.
void HM_FUN(Iter)(const HASHMAP_NAME*, HM_FUN(Iterator)*, void* userdata);

#endif

#undef _HM_MAKE_FN_NAME
#undef _HM_FUN
#undef HM_FUN
