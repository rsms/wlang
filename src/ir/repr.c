#include "../wp.h"
#include "ir.h"

typedef struct {
  Str  buf;
  bool includeTypes;
} IRRepr;


static void reprValue(IRRepr* r, const IRValue* v) {
  assert(v->op < Op_MAX);
  r->buf = sdscatprintf(r->buf,
    "    v%-2u = %-*s",
    v->id,
    IROpNamesMaxLen,
    IROpNames[v->op]
  );
  // args
  for (u8 i = 0; i < v->argslen; i++) {
    r->buf = sdscatprintf(r->buf, i+1 < v->argslen ? " v%-2u " : " v%u", v->args[i]->id);
  }
  // aux
  auto opinfo = IROpInfo(v->op);
  switch (opinfo->aux) {
    case IRAuxNone:
      break;
    case IRAuxBool:
    case IRAuxI8:
    case IRAuxI16:
    case IRAuxI32:
      r->buf = sdscatprintf(r->buf, " [0x%X]", v->auxInt);
      break;
    case IRAuxF32:
      r->buf = sdscatprintf(r->buf, " [%f]", *(f32*)(&v->auxInt));
      break;
    case IRAuxI64:
      r->buf = sdscatprintf(r->buf, " [0x%llX]", v->auxInt);
      break;
    case IRAuxF64:
      r->buf = sdscatprintf(r->buf, " [%f]", *(f64*)(&v->auxInt));
      break;
  }
  r->buf = sdscatlen(r->buf, "\n", 1);
}


static void reprBlock(IRRepr* r, const IRBlock* b) {
  r->buf = sdscatfmt(r->buf, "  b%u\n", b->id);
  ArrayForEach(&b->values, IRValue*, v, {
    reprValue(r, v);
  });
}


static void reprFun(IRRepr* r, const IRFun* f) {
  r->buf = sdscatfmt(r->buf,
    "fun %s %s\n",
    f->name == NULL ? "_" : f->name,
    f->typeid == NULL ? "()" : f->typeid
  );
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
