#include "wp.h"

void parsefile(Str filename) {

  // load file contents
  size_t len;
  auto buf = readfile(filename, &len);
  if (!buf) {
    die("%s: %s", filename, strerror(errno));
  }

  Source src;
  SourceInit(&src, filename, buf, len);

  P p;
  PInit(&p, &src);
  PParseFile(&p);

  // ssize_t n;
  // u8 buf[BUFSIZ];
  // int out = 1; // stdout
  // while ((n = read(fd, buf, BUFSIZ)) > 0) {
  //   ScannerScan(s, buf, n);
  //   while (n > 0) {
  //     auto m = write(out, buf, n);
  //     if (m < 0) {
  //       die("write error: %s", strerror(errno));
  //       exit(1);
  //     }
  //     n -= m;
  //   }
  // }
  // if (n < 0) {
  //   die("read error: %s", strerror(errno));
  // }

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

  parsefile(sdsnew(argv[1]));

  return 0;
}

