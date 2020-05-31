#include "../wp.h"
#include "ir.h"


IRPkg* IRPkgNew(FWAllocator* a, const char* name) {
  size_t namelen = name == NULL ? 0 : (strlen(name) + 1);
  auto pkg = (IRPkg*)FWAlloc(a, sizeof(IRPkg) + namelen);

  pkg->mem = a;

  ArrayInitWithStorage(&pkg->funs, pkg->funsStorage, sizeof(pkg->funsStorage)/sizeof(void*));

  if (name == NULL) {
    pkg->name = "_";
  } else {
    char* name2 = ((char*)pkg) + namelen;
    memcpy(name2, name, namelen);
    name2[namelen] = 0;
    pkg->name = name2;
  }

  return pkg;
}


void IRPkgAddFun(IRPkg* pkg, IRFun* f) {
  ArrayPush(&pkg->funs, f, pkg->mem);
}
