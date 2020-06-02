#include "../wp.h"
#include "ir.h"

typedef struct {
  Str  buf;
  bool includeTypes;
} IRRepr;


static void reprValue(IRRepr* r, const IRValue* v) {
  assert(v->op < Op_MAX);

  // vN type = Op
  r->buf = sdscatprintf(r->buf,
    "    v%-2u %-7s = %-*s",
    v->id,
    TypeCodeName(v->type),
    IROpNamesMaxLen,
    IROpNames[v->op]
  );

  // arg arg
  for (u8 i = 0; i < v->argslen; i++) {
    r->buf = sdscatprintf(r->buf, i+1 < v->argslen ? " v%-2u " : " v%u", v->args[i]->id);
  }

  // [auxInt]
  auto opinfo = IROpInfo(v->op);
  switch (opinfo->aux) {
    case IRAuxNone:
      break;
    case IRAuxBool:
    case IRAuxI8:
    case IRAuxI16:
    case IRAuxI32:
      r->buf = sdscatprintf(r->buf, " [0x%X]", (u32)v->auxInt);
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

  // {aux}
  // TODO non-numeric aux

  // comment
  if (v->comment != NULL) {
    r->buf = sdscatfmt(r->buf, "\t# %u use ; %s", v->uses, v->comment);
  } else {
    r->buf = sdscatfmt(r->buf, "\t# %u use", v->uses);
  }

  r->buf = sdscatlen(r->buf, "\n", 1);
}


static void reprBlock(IRRepr* r, const IRBlock* b) {
  r->buf = sdscatfmt(r->buf, "  b%u:\n", b->id);
  // TODO: predecessors, e.g. "b3: <- b1, b4"
  ArrayForEach(&b->values, IRValue*, v, {
    reprValue(r, v);
  });

  // TODO: control and successors e.g. "if v4 -> b2, b1"
  switch (b->kind) {
  case IRBlockInvalid:
    r->buf = sdscat(r->buf, "  ?\n");
    break;

  case IRBlockCont:
    r->buf = sdscatfmt(r->buf, "  cont\n");
    // TODO successors
    break;

  case IRBlockFirst:
    r->buf = sdscatfmt(r->buf, "  first\n");
    break;

  case IRBlockIf:
    r->buf = sdscatfmt(r->buf, "  if\n");
    break;

  case IRBlockRet:
    assert(b->control != NULL);
    r->buf = sdscatfmt(r->buf, "  ret v%u\n", b->control->id);
    break;
  }
}


static void reprFun(IRRepr* r, const IRFun* f) {
  r->buf = sdscatprintf(r->buf,
    "fun %s %s %p\n",
    f->name == NULL ? "_" : f->name,
    f->typeid == NULL ? "()" : f->typeid,
    f
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
