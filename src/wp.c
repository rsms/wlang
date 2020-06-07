#include "wp.h"
#include "ir/builder.h"

static void errorHandler(const Source* src, SrcPos pos, ConstStr msg, void* userdata) {
  u32* errcount = (u32*)userdata;
  (*errcount)++;
  auto s = SrcPosMsg(sdsempty(), pos, msg);
  s[sdslen(s)-1] = '\n'; // repurpose NUL
  fwrite(s, sdslen(s), 1, stderr);
  sdsfree(s);
}


static void printAst(const Node* n) {
  auto s = NodeRepr(n, sdsempty());
  s = sdscatlen(s, "\n", 1);
  fwrite(s, sdslen(s), 1, stdout);
  sdsfree(s);
}


static void printIR(const IRPkg* pkg) {
  auto s = IRReprPkgStr(pkg, sdsempty());
  s = sdscatlen(s, "\n", 1);
  fwrite(s, sdslen(s), 1, stdout);
  sdsfree(s);
}


void parsefile(Str filename, Scope* pkgscope) {

  // load file contents
  size_t len;
  auto buf = os_readfile(filename, &len, NULL);
  if (!buf) {
    die("%s: %s", filename, strerror(errno));
  }

  // our userdata is number of errors encountered (incremented by errorHandler)
  u32 errcount = 0;

  // compilation context
  CCtx cc = {0}; // TODO: share across individual, non-overlapping compile sessions
  CCtxInit(&cc, errorHandler, &errcount, filename, buf, len);

  printf("————————————————————————————————————————————————————————————————\n");
  printf("PARSE\n");
  // parse input
  static P parser; // shared parser (zero-initialized since it's static)
  auto file = Parse(&parser, &cc, ParseComments /*| ParseOpt*/, pkgscope);
  printAst(file);
  if (errcount != 0) { goto end; }

  // resolve symbols and types
  if (parser.unresolved == 0) {
    printf("(no unresolved names; not running sym resolver)\n");
  } else {
    printf("————————————————————————————————————————————————————————————————\n");
    printf("RESOLVE NAMES\n");
    ResolveSym(&cc, parser.s.flags, file, pkgscope);
    printAst(file);
    if (errcount != 0) { goto end; }
  }

  printf("————————————————————————————————————————————————————————————————\n");
  printf("RESOLVE TYPES\n");
  ResolveType(&cc, file);
  printAst(file);
  if (errcount != 0) { goto end; }

  printf("————————————————————————————————————————————————————————————————\n");
  printf("BUILD IR\n");
  // build some IR
  IRBuilder irbuilder = {};
  IRBuilderInit(&irbuilder, IRBuilderComments /*| IRBuilderOpt*/, "foo"); // start a new package
  IRBuilderAdd(&irbuilder, &cc, file); // add ast to current package

  printf("————————————————————————————————————————————————————————————————\n");
  // print IR SLC
  printIR(irbuilder.pkg);

  end:
  CCtxFree(&cc);
  TmpRecycle();
}


int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <input>...\n", argv[0]);
    exit(1);
  }

  // int out = 1; // stdout
  // TODO: support -o <file> CLI flag.
  // int out = open(argv[2], O_WRONLY | O_CREAT, 0660);
  // if (out < 0) {
  //   fprintf(stderr, "error opening output %s: %s\n", argv[2], strerror(errno));
  //   exit(1);
  // }

  auto pkgscope = ScopeNew(GetGlobalScope(), NULL);
  parsefile(sdsnew(argv[1]), pkgscope);

  return 0;
}

