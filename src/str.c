#include "wp.h"

// strrepr returns a printable representation of an sds string (sds, Sym, etc.)
// using sdscatrepr which encodes non-printable ASCII chars for safe printing.
// Not thread safe!
const char* strrepr(ConstStr s) {
  static Str buf;
  if (buf == NULL) {
    buf = sdsempty();
  } else {
    sdssetlen(buf, 0);
  }
  buf = sdscatrepr(buf, s, sdslen(s));
  return buf;
}
