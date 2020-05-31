#include "memory.h"
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
  u32    cap;     // number of buckets
  u32    len;     // number of key-value entries
  u32    flags;   //
  Memory mem;     // memory allocator. NULL = use global allocator
  void*  buckets; // internal
} HASHMAP_NAME;

#ifdef HASHMAP_INCLUDE_DECLARATIONS
// Include declarations.
// Normally these are copy-pasted and hand-converted in the user-level header.

// New creates a new map with initbuckets intial buckets.
HASHMAP_NAME* HM_FUN(New)(u32 initbuckets, Memory)

// Free frees all memory of a map, including the map's memory.
// Use Free when you created a map with New.
// Use Dealloc when you manage the memory of the map yourself and used Init.
void HM_FUN(Free)(HASHMAP_NAME*);

// Init initializes a map structure. initbuckets is the number of initial buckets.
void HM_FUN(Init)(HASHMAP_NAME*, u32 initbuckets, Memory);

// Dealloc frees buckets data (but not the hashmap itself.)
// The hashmap is invalid after this call. Call Init to reuse.
void HM_FUN(Dealloc)(HASHMAP_NAME*);

// Get searches for key. Returns value, or NULL if not found.
HASHMAP_VALUE HM_FUN(Get)(const HASHMAP_NAME*, HASHMAP_KEY key);

// Set inserts key=value into m. Returns the replaced value or NULL if not found.
HASHMAP_VALUE HM_FUN(Set)(HASHMAP_NAME*, HASHMAP_KEY key, HASHMAP_VALUE value);

// Del removes value for key. Returns the removed value or NULL if not found.
HASHMAP_VALUE HM_FUN(Del)(HASHMAP_NAME*, HASHMAP_KEY key);

// Clear removes all entries. In contrast to Free, map remains valid.
void HM_FUN(Clear)(HASHMAP_NAME*);

// Iterator function type. Set stop=true to stop iteration.
typedef void(HM_FUN(Iterator))(HASHMAP_KEY key, HASHMAP_VALUE value, bool* stop, void* userdata);

// Iter iterates over entries of the map.
void HM_FUN(Iter)(const HASHMAP_NAME*, HM_FUN(Iterator)*, void* userdata);

#endif

#undef _HM_MAKE_FN_NAME
#undef _HM_FUN
#undef HM_FUN
