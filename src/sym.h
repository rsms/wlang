#pragma once
#include "token.h"
#include "types.h"

// Sym is a type of sds string, compatible with sds functions, with an additional header
// containing a precomputed FNV1a hash. Sym is immutable.
typedef const char* Sym;

// Predefinition of Node
typedef struct Node Node;

// Get a symbol (retrieves or interns)
Sym symget(const u8* data, size_t len, u32 hash);

// Hashes data and then calls symget
Sym symgeth(const u8* data, size_t len);

// Compare two Sym's string values.
inline static int symcmp(Sym a, Sym b) { return a == b ? 0 : strcmp(a, b); }

typedef struct __attribute__((__packed__)) SymHeader {
  u32             hash;
  struct sdshdr16 sh;
} SymHeader;

// access SymHeader from Sym/sds/const char*
#define SYM_HDR(s) ((const SymHeader*)((s) - (sizeof(SymHeader))))

// access FNV1a hash of s
inline static u32 symhash(Sym s) { return SYM_HDR(s)->hash; }

// faster alternative to sdslen, without type lookup
inline static u16 symlen(Sym s) { return SYM_HDR(s)->sh.len; }

// Returns the Tok representing this sym in the language syntax.
// Either returns a keyword token or TIdent if s is not a keyword.
inline static Tok symLangTok(Sym s) {
  // Bits 4-8 represents offset into Tok enum when s is a language keyword.
  u32 kwindex = SYM_HDR(s)->sh.flags >> SDS_TYPE_BITS;
  return kwindex == 0 ? TIdent : kwindex + TKeywordsStart;
}

// SymMap maps Sym to pointers
#define HASHMAP_NAME     SymMap
#define HASHMAP_KEY      Sym
#define HASHMAP_VALUE    void*
#include "hashmap.h"
#undef HASHMAP_NAME
#undef HASHMAP_KEY
#undef HASHMAP_VALUE

// Creates and initializes a new SymMap in mem, or global memory if mem is NULL.
SymMap* SymMapNew(u32 initbuckets, Memory mem/*null*/);

// SymMapInit initializes a map structure. initbuckets is the number of initial buckets.
void SymMapInit(SymMap*, u32 initbuckets, Memory mem/*null*/);

// SymMapFree frees SymMap along with its data.
void SymMapFree(SymMap*);

// SymMapDealloc frees heap memory used by a map, but leaves SymMap untouched.
void SymMapDealloc(SymMap*);

// SymMapGet searches for key. Returns value, or NULL if not found.
void* SymMapGet(const SymMap*, Sym key);

// SymMapSet inserts key=value into m. Returns the replaced value or NULL if not found.
void* SymMapSet(SymMap*, Sym key, void* value);

// SymMapDel removes value for key. Returns the removed value or NULL if not found.
void* SymMapDel(SymMap*, Sym key);

// SymMapClear removes all entries. In contrast to SymMapFree, map remains valid.
void SymMapClear(SymMap*);

// Iterator function type. Set stop=true to stop iteration.
typedef void(SymMapIterator)(Sym key, void* value, bool* stop, void* userdata);

// SymMapIter iterates over entries of the map.
void SymMapIter(const SymMap*, SymMapIterator*, void* userdata);


// symbols for language keywords (defined in token.h)
#define SYM_DEF(str, _) \
  const Sym sym_##str;
TOKEN_KEYWORDS(SYM_DEF)
#undef SYM_DEF


// symbols and AST nodes for predefined types (defined in types.h)
#define SYM_DEF(name) \
  const Sym sym_##name; \
  Node* Type_##name;
TYPE_SYMS(SYM_DEF)
#undef SYM_DEF

// nil is special and implemented without macros since its sym is defined by TOKEN_KEYWORDS
Node* Type_nil;
Node* Const_nil;


// symbols and AST nodes for predefined constants
#define PREDEFINED_CONSTANTS(_) \
  _( true,  bool, 1 ) \
  _( false, bool, 0 ) \
/*END PREDEFINED_CONSTANTS*/
#define SYM_DEF(name, _type, _val) \
  const Sym sym_##name; \
  Node* Const_##name;
PREDEFINED_CONSTANTS(SYM_DEF)
#undef SYM_DEF


// symbols for predefined common identifiers
// predefined common identifiers (excluding types)
#define PREDEFINED_IDENTS(ID) \
  ID( _ ) \
/*END PREDEFINED_IDENTS*/
#define SYM_DEF(name) \
  const Sym sym_##name;
PREDEFINED_IDENTS(SYM_DEF)
#undef SYM_DEF
