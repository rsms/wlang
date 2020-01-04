#pragma once
#include "token.h"

// Sym is an type of sds string with an additional header containing
// a precomputed FNV1a hash. Sym is immutable.
typedef const char* Sym;

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

// SymMap maps Sym to void*
#define HASHMAP_NAME     SymMap
#define HASHMAP_KEY      Sym
#define HASHMAP_VALUE    void*
#include "hashmap.h"
#undef HASHMAP_NAME
#undef HASHMAP_KEY
#undef HASHMAP_VALUE

// SymMapInit initializes a map structure. initbuckets is the number of initial buckets.
void SymMapInit(SymMap*, size_t initbuckets);

// SymMapFree frees buckets data.
void SymMapFree(SymMap*);

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


// predefined symbols
const Sym sym_break;
const Sym sym_case;
const Sym sym_const;
const Sym sym_continue;
const Sym sym_default;
const Sym sym_defer;
const Sym sym_else;
const Sym sym_enum;
const Sym sym_fallthrough;
const Sym sym_for;
const Sym sym_fun;
const Sym sym_go;
const Sym sym_if;
const Sym sym_import;
const Sym sym_in;
const Sym sym_interface;
const Sym sym_is;
const Sym sym_return;
const Sym sym_select;
const Sym sym_struct;
const Sym sym_switch;
const Sym sym_symbol;
const Sym sym_type;
const Sym sym_var;
const Sym sym_while;
const Sym sym__;
const Sym sym_int;
