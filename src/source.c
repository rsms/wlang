#include "wp.h"


void SourceInit(Source* s, Str name, const u8* buf, size_t len) {
  s->name = sdsdup(name);
  s->buf = buf;
  s->len = len;
  s->_lineoffsets = NULL;
  s->_linecount = 0;
}


void SourceFree(Source* s) {
  sdsfree(s->name);
  s->name = NULL;
  if (s->_lineoffsets) {
    free(s->_lineoffsets);
    s->_lineoffsets = NULL;
  }
}


static void computeLineOffsets(Source* s) {
  assert(s->_lineoffsets == NULL);

  size_t cap = 256; // best guess for common line numbers, to allocate up-front
  s->_lineoffsets = (u32*)malloc(sizeof(u32) * cap);
  s->_lineoffsets[0] = 0;

  u32 linecount = 1;
  u32 i = 0;
  while (i < s->len) {
    if (s->buf[i++] == '\n') {
      if (linecount == cap) {
        // more lines
        cap = cap * 2;
        s->_lineoffsets = (u32*)realloc(s->_lineoffsets, sizeof(u32) * cap);
      }
      s->_lineoffsets[linecount] = i;
      linecount++;
    }
  }

  s->_linecount = linecount;
}


LineCol SrcPosLineCol(SrcPos pos) {
  Source* s = pos.src;
  if (!s->_lineoffsets) {
    computeLineOffsets(s);
  }

  if (pos.offs >= s->len) { dlog("pos.offs=%u >= s->len=%zu", pos.offs, s->len); }
  assert(pos.offs < s->len);

  u32 count = s->_linecount;
  u32 line = 0;
  u32 debug1 = 10;
  while (count > 0 && debug1--) {
    u32 step = count / 2;
    u32 i = line + step;
    if (s->_lineoffsets[i] <= pos.offs) {
      line = i + 1;
      count = count - step - 1;
    } else {
      count = step;
    }
  }
  LineCol r = { line - 1, line > 0 ? pos.offs - s->_lineoffsets[line - 1] : pos.offs };
  return r;
}


static const u8* lineContents(Source* s, u32 line, u32* out_len) {
  if (!s->_lineoffsets) {
    computeLineOffsets(s);
  }
  if (line >= s->_linecount) {
    return NULL;
  }
  auto start = s->_lineoffsets[line];
  const u8* lineptr = s->buf + start;
  if (out_len) {
    if (line + 1 < s->_linecount) {
      *out_len = (s->_lineoffsets[line + 1] - 1) - start;
    } else {
      *out_len = (s->buf + s->len) - lineptr;
    }
  }
  return lineptr;
}


Str SrcPosFmt(Str s, SrcPos pos) {
  auto l = SrcPosLineCol(pos);
  return sdscatfmt(s, "%s:%u:%u",
    pos.src ? pos.src->name : sdsnew("<input>"), l.line + 1, l.col + 1);
}


Str SrcPosMsg(Str s, SrcPos pos, CStr message) {
  auto l = SrcPosLineCol(pos);
  s = sdscatfmt(s, "%s:%u:%u: %S\n",
    pos.src ? pos.src->name : sdsnew("<input>"), l.line + 1, l.col + 1,
    message
  );

  // include line contents
  if (pos.src) {
    u32 linelen;
    auto lineptr = lineContents(pos.src, l.line, &linelen);
    if (lineptr != null) {
      s = sdscatlen(s, lineptr, linelen);
    }
    s = sdscatlen(s, "\n", 1);

    // draw a squiggle (or caret when span is unknown) decorating the interesting range
    if (l.col > 0) {
      // indentation
      s = sdsgrow(s, sdslen(s) + l.col, ' ');
    }
    if (pos.span > 0) {
      s = sdsgrow(s, sdslen(s) + pos.span + 1, '~');
      s[sdslen(s)-1] = '\n';
    } else {
      s = sdscatlen(s, "^\n", 2);
    }
  }

  return s;
}

