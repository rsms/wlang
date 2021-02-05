#pragma once
#include "sds.h"
#include "defs.h"

#define Str      sds
#define ConstStr constsds

inline static Str strgrow(Str nonull s, size_t addlSize) {
  return sdsMakeRoomFor(s, align2(addlSize, 128));
}

// true if s starts with C-string prefix
bool strhasprefix(ConstStr nonull s, const char* nonull prefix);

// strrepr returns a printable representation of an sds string (sds, Sym, etc.)
// using sdscatrepr which encodes non-printable ASCII chars for safe printing.
// E.g. "foo\x00bar" if the string contains a zero byte.
// Returns a garbage-collected string.
ConstStr bytesrepr(const u8* s, size_t len);
inline static ConstStr strrepr(ConstStr s) {
  return bytesrepr((const u8*)s, sdslen(s));
}
