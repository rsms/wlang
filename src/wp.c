#include "wp.h"

static void errorHandler(Source* src, SrcPos pos, CStr msg, void* userdata) {
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


void parsefile(Str filename, Scope* pkgscope) {

  // load file contents
  size_t len;
  auto buf = readfile(filename, &len);
  if (!buf) {
    die("%s: %s", filename, strerror(errno));
  }

  Source src;
  SourceInit(&src, filename, buf, len);

  // shared parser
  static P parser;

  u32 errcount = 0;
  auto file = Parse(&parser, &src, SCAN_COMMENTS, errorHandler, pkgscope, &errcount);
  // printAst(file);

  if (errcount == 0) {
    Resolve(file, &src, errorHandler, &errcount);
    if (errcount == 0) {
      printAst(file);
    }
  } else {
    printAst(file);
  }

  free(buf);
  SourceFree(&src);
}


int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <input>...\n", argv[0]);
    exit(1);
  }

  int out = 1; // stdout
  // TODO: support -o <file> CLI flag.
  // int out = open(argv[2], O_WRONLY | O_CREAT, 0660);
  // if (out < 0) {
  //   fprintf(stderr, "error opening output %s: %s\n", argv[2], strerror(errno));
  //   exit(1);
  // }


  auto pkgscope = ScopeNew(GetGlobalScope());
  parsefile(sdsnew(argv[1]), pkgscope);

  return 0;
}

