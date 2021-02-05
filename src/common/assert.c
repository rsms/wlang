#include <execinfo.h>

#include "assert.h"
#include "os.h"
#include "tstyle.h"

#include "test.h"


static bool fprintSourceFile(
  FILE* nonull fp,
  const char* nonull file,
  u32 line,
  u32 contextLines,
  bool colors
) {
  // try to read source file
  size_t srclen = 1024*1024; // read limit
  auto srcbuf = os_readfile(file, &srclen, NULL);
  if (srcbuf == NULL) {
    return false;
  }
  int len = (int)srclen;
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


static void fprintStackTrace(FILE* nonull fp, int offsetFrames) {
  // try to show stack trace
  void* callstack[200];
  int framecount = backtrace(callstack, countof(callstack));
  if (framecount > 0) {
    char** strs = backtrace_symbols(callstack, framecount);
    if (strs != NULL) {
      fprintf(fp, "Call stack:\n");
      for (int i = offsetFrames + 1; i < framecount; ++i) {
        fprintf(fp, "  %s\n", strs[i]);
      }
      free(strs);
    }
  }
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


const char* _assert_joinstr(const char* s1, ... /* NULL terminated */) {
  static char buf[256] = {0};
  char* p = buf;

  size_t len = strlen(s1);
  memcpy(p, s1, len);
  p += len;

  va_list ap;
  va_start(ap, s1);
  while (1) {
    const char* s = va_arg(ap, const char*);
    if (s == NULL) {
      break;
    }
    len = strlen(s);
    memcpy(p, s, len);
    p += len;
    *p = '\0';
  }
  va_end(ap);
  assertf((buf + sizeof(buf)) > p, "overflow");
  *p = '\0';
  return buf;
}


// Note: Since this prints to stderr, only enable this in the "test" product
#ifdef W_TEST_BUILD
  W_UNIT_TEST(Assert, {
    const char* pch = _assert_joinstr("aa", "bb", "cc", NULL);
    assert(memcmp(pch, "aabbcc", 6) == 0);
    asserteq(pch[6], 0);

    // non-existant file
    fprintf(stdout, "----- START TEST OUTPUT -----\n");
    fprintSourceFile(stdout, __FILE__ ".xxx", 1, /* contextLines */ 3, /*colors*/ true);
    // no colors, no linebreak at end
    fprintSourceFile(stdout, "test/file-no-final-line-break",
      2, /* contextLines */ 3, /*colors*/ false);

    fprintf(stdout, "----- THE BELOW ASSERTION IS EXPECTED TO FAIL -----\n");
    WAssertf(__FILE__, __LINE__, "%s:%d: test %d", __FILE__, __LINE__, 123);
    fprintf(stdout, "----- THE ABOVE ASSERTION IS EXPECTED TO FAIL -----\n");
    fprintf(stdout, "----- END TEST OUTPUT -----\n");
  })
#endif
