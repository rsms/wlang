// PtrMap maps void* to void*. sizeof(PtrMap) == 3*sizeof(void*)
#define HASHMAP_NAME     PtrMap
#define HASHMAP_KEY      void*
#define HASHMAP_VALUE    void*
#include "hashmap.h"
#undef HASHMAP_NAME
#undef HASHMAP_KEY
#undef HASHMAP_VALUE

// PtrMapInit initializes a map structure. initbuckets is the number of initial buckets.
void PtrMapInit(PtrMap*, size_t initbuckets, Memory mem/*nullable*/);

// bool PtrMapIsInit(PtrMap*)
#define PtrMapIsInit HASHMAP_IS_INIT

// PtrMapFree frees buckets data.
void PtrMapFree(PtrMap*);

// PtrMapGet searches for key. Returns value, or NULL if not found.
void* PtrMapGet(const PtrMap*, void* key);

// PtrMapSet inserts key=value into m. Returns the replaced value or NULL if not found.
void* PtrMapSet(PtrMap*, void* key, void* value);

// PtrMapDel removes value for key. Returns the removed value or NULL if not found.
void* PtrMapDel(PtrMap*, void* key);

// PtrMapClear removes all entries. In contrast to PtrMapFree, map remains valid.
void PtrMapClear(PtrMap*);

// Iterator function type. Set stop=true to stop iteration.
typedef void(PtrMapIterator)(void* key, void* value, bool* stop, void* userdata);

// PtrMapIter iterates over entries of the map.
void PtrMapIter(const PtrMap*, PtrMapIterator*, void* userdata);

