#include <execinfo.h>

#include "defs.h"
#include "os.h"
#include "tstyle.h"

#include "test.h"


static bool fprintSourceFile(
  FILE* nonull fp,
  const char* nonull file,
  int line,
  int contextLines,
  bool colors
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
        fprintf(fp, "%s%-4d >%s %.*s\n",
          colors ? TStyleTable[TStyle_inverse] : "",
          lineno,
          colors ? TStyle_none : "",
          i - linestart,
          &srcbuf[linestart]
        );
      } else if (linemin <= lineno && lineno <= linemax) {
        fprintf(fp, "%-4d   %.*s\n", lineno, i - linestart, &srcbuf[linestart]);
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


void WAssertf(const char* srcfile, int srcline, const char* nonull format, ...) {
  bool colors = TSTyleStderrIsTTY();
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc('\n', stderr);
  if (srcfile != NULL) {
    fprintSourceFile(stderr, srcfile, srcline, /* contextLines */ 3, colors);
  }
  fprintStackTrace(stderr, /* offsetFrames = */ 1);
}


// Note: Since this prints to stderr, only enable this in the "test" product
#ifdef W_TEST_BUILD
  W_UNIT_TEST(Assert, {
    fprintf(stderr, "----- THE BELOW ASSERTION IS EXPECTED TO FAIL -----\n");
    WAssertf(__FILE__, __LINE__, "%s:%d: test %d", __FILE__, __LINE__, 123);
    fprintf(stderr, "----- THE ABOVE ASSERTION IS EXPECTED TO FAIL -----\n");
  })
#endif
