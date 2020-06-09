#include "wp.h"

// strrepr returns a printable representation of an sds string (sds, Sym, etc.)
// using sdscatrepr which encodes non-printable ASCII chars for safe printing.
// Not thread safe!
ConstStr strrepr(ConstStr s) {
  auto len = sdslen(s);
  return memgcsds(sdscatrepr(sdsnewcap(len + 2), s, len));
}

bool strhasprefix(ConstStr s, const char* prefix) {
  size_t plen = strlen(prefix);
  return sdslen(s) < plen ? false : memcmp(s, prefix, plen) == 0;
}


W_UNIT_TEST(Str, {
  assert(strcmp( strrepr(sdsnew("lolcat")), "\"lolcat\"" ) == 0);
  assert(strcmp( strrepr(sdsnew("lol\"cat")), "\"lol\\\"cat\"" ) == 0);
  assert(strcmp( strrepr(sdsnew("lol\ncat")), "\"lol\\ncat\"" ) == 0);
  assert(strcmp( strrepr(sdsnew("lol\x01 cat")), "\"lol\\x01 cat\"" ) == 0);

  assert(strhasprefix(sdsnew("lolcat"), "lol") == true);
  assert(strhasprefix(sdsnew("lol"),    "lol") == true);
  assert(strhasprefix(sdsnew("lo"),     "lol") == false);
})
