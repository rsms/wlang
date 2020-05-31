#include "../wp.h"
#include "ir.h"

typedef struct {
  Str  buf;
  bool includeTypes;
} IRRepr;


static void reprValue(IRRepr* r, const IRValue* v) {
  assert(v->op < Op_MAX);
  r->buf = sdscatfmt(r->buf, "    v%u  %s\n", v->id, IROpNames[v->op]);
}


static void reprBlock(IRRepr* r, const IRBlock* b) {
  r->buf = sdscatfmt(r->buf, "  b%u\n", b->id);
  ArrayForEach(&b->values, IRValue*, v, {
    reprValue(r, v);
  });
}


static void reprFun(IRRepr* r, const IRFun* f) {
  r->buf = sdscatfmt(r->buf, "fun %s\n", f->name == NULL ? "_" : f->name);
  ArrayForEach(&f->blocks, IRBlock*, b, {
    reprBlock(r, b);
  });
}


static void reprPkg(IRRepr* r, const IRPkg* pkg) {
  r->buf = sdscatfmt(r->buf, "package %s\n", pkg->name);
  ArrayForEach(&pkg->funs, IRFun*, f, {
    reprFun(r, f);
  });
}


Str IRReprPkgStr(const IRPkg* pkg, Str init) {
  IRRepr r = { .buf=init, .includeTypes=true };
  if (r.buf == NULL) {
    r.buf = sdsempty();
  }
  reprPkg(&r, pkg);
  return r.buf;
}
