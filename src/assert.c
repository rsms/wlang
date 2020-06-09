#include <execinfo.h>
#include "defs.h"
#include "os.h"


static bool fprintSourceFile(
  FILE* nonull fp,
  const char* nonull file,
  int line,
  int contextLines
) {
  // try to read source file
  size_t srclen = 1024*1024; // read limit
  auto srcbuf = os_readfile(file, &srclen, NULL);
  if (srcbuf == NULL) {
    return false;
  }
  if (contextLines < 0) { contextLines = 0; }
  int len = srclen > 0x7fffffff ? 0x7fffffff : (int)srclen;
  int lineno = 1;
  int linemin = max(0, line - contextLines);
  int linemax = line + contextLines;
  int start = -1;
  int end = -1;
  int linestart = 0;
  bool tail = true;
  for (int i = 0; i < len; i++) {
    if (srcbuf[i] == '\n') {
      if (lineno == linemin) {
        start = linestart;
      }
      if (lineno == line) {
        fprintf(fp, "% 4d > %.*s\n", lineno, i - linestart, &srcbuf[linestart]);
      } else if (linemin <= lineno && lineno <= linemax) {
        fprintf(fp, "% 4d   %.*s\n", lineno, i - linestart, &srcbuf[linestart]);
      }
      if (lineno == linemax) {
        end = i;
        tail = false;
        break;
      }
      lineno++;
      linestart = i + 1;
    }
  }
  if (tail) { // no linebreak at end of file
    fprintf(fp, "% 4d   %.*s\n", lineno, len - linestart, &srcbuf[linestart]);
  }
  return true;
}


static bool fprintStackTrace(FILE* nonull fp, int offsetFrames) {
  // try to show stack trace
  void* callstack[4096];
  int framecount = backtrace(callstack, countof(callstack));
  if (framecount <= 0) {
    return false;
  }
  char** strs = backtrace_symbols(callstack, framecount);
  if (strs == NULL) {
    return false;
  }
  fprintf(fp, "Call stack:\n");
  for (int i = offsetFrames + 1; i < framecount; ++i) {
    fprintf(fp, "  %s\n", strs[i]);
  }
  free(strs);
  return true;
}


void _wassert(bool cond, const char* msg, const char* file, int line) {
  if (cond) {
    return;
  }
  fprintf(stderr, "%s:%d: Assertion error: %s\n", file, line, msg);
  fprintSourceFile(stderr, file, line, /* contextLines */ 3);
  fprintStackTrace(stderr, /* offsetFrames = */ 1);
  abort();
}

