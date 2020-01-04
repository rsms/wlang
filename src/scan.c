#include "wp.h"

#include "token.h"

// Enable to print "D >> TOKEN VALUE at SOURCELOC" on each call to SNext
// #define SCANNER_DEBUG_TOKEN_PRODUCTION


const char* TokName(Tok t) {
  switch (t) {
    case TNil: return "TNil";

    #define I_ENUM(name, str) case name: return str;
    TOKEN_TYPES_1_1(I_ENUM)
    #undef I_ENUM

    case TSymbolicStart: return "TSymbolicStart";

    #define I_ENUM(name, str) case name: return str;
    TOKEN_TYPES(I_ENUM)
    #undef I_ENUM

    case TKeywordsStart: return "TKeywordsStart";
    #define I_ENUM(str, name) case name: return "keyword." #str;
    TOKEN_KEYWORDS(I_ENUM)
    #undef I_ENUM
    case TKeywordsEnd: return "TKeywordsEnd";

    case TMax: return "TMax";
  }
}


#ifndef NDEBUG
__attribute__((constructor)) static void init_debug() {
  // We only have 5 bits to encode tokens in Sym. Additionally, the value 0 is reserved
  // for "not a keyword", leaving the max number of values at 31 (i.e. 2^5=32-1).
  assert(TKeywordsEnd - TKeywordsStart - 1 < 32);
}
#endif


// character flags. (bit flags)
// * + - . / 0-9 A-Z _ a-z
//
#define CH_IDENT      1 << 0  /* 1 = valid in middle of identifier */
#define CH_WHITESPACE 1 << 1  /* 2 = whitespace */
//
static u8 charflags[256] = {
        /* 0 1 2 3 4 5 6 7 8 9 A B C D E F */
/* 0x00 */ 0,0,0,0,0,0,0,0,0,2,2,0,0,2,0,0,  // <CTRL> ... 9=TAB, A=LF, D=CR
/* 0x10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <CTRL> ...
/* 0x20 */ 2,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1,  //   ! " # $ % & ' ( ) * + , - . /
/* 0x30 */ 1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,  // 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
/* 0x40 */ 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  // @ A B C D E F G H I J K L M N O
/* 0x50 */ 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,  // P Q R S T U V W X Y Z [ \ ] ^ _
/* 0x60 */ 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  // ` a b c d e f g h i j k l m n o
/* 0x70 */ 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,  // p q r s t u v w x y z { | } ~ <DEL>
/* 0x80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <CTRL> ...
/* 0x90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <CTRL> ...
/* 0xA0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // <NBSP> ¡ ¢ £ ¤ ¥ ¦ § ¨ © ª « ¬ <SOFTHYPEN> ® ¯
/* 0xB0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // ° ± ² ³ ´ µ ¶ · ¸ ¹ º » ¼ ½ ¾ ¿
/* 0xC0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï
/* 0xD0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß
/* 0xE0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // à á â ã ä å æ ç è é ê ë ì í î ï
/* 0xF0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ
};


void SInit(S* s, Source* src, ScanFlags flags, ErrorHandler* errh, void* userdata) {
  memset(s, 0, sizeof(S));

  s->src   = src;
  s->inp   = src->buf;
  s->inp0  = src->buf;
  s->inend = src->buf + src->len;
  s->flags = flags;

  s->tok      = TNil;
  s->tokstart = 0;
  s->tokend   = 0;

  s->lineno = 0;
  s->linestart = s->inp;

  s->errh = errh;
  s->userdata = userdata;
}


static void serr(S* s, const char* format, ...) {
  auto pos = SSrcPos(s);
  auto msg = SrcPosFmt(sdsempty(), pos);
  msg = sdscatlen(msg, ": ", 2);

  va_list ap;
  va_start(ap, format);
  msg = sdscatvprintf(msg, format, ap);
  va_end(ap);

  if (s->errh) {
    s->errh(s->src, pos, msg, s->userdata);
  } else {
    msg[sdslen(msg)] = '\n'; // replace NUL with ln
    fwrite(msg, sdslen(msg)+1, 1, stderr);
  }

  sdsfree(msg);
}


// // unreadrune sets the reading position to a previous reading position, usually the one of
// // the most recently read rune, but possibly earlier (see unread below).
// inline static void unreadrune(S* s) {
//   s->inp = s->inp0;
// }


static void addComment(S* s) {
  auto c = (Comment*)malloc(sizeof(Comment));
  c->next = NULL;
  c->src = s->src;
  c->ptr = s->tokstart;
  c->len = s->tokend - s->tokstart;

  if (s->comments) {
    s->comments_tail->next = c;
  } else {
    s->comments = c;
  }
  s->comments_tail = c;
}


static void scomment(S* s) {
  s->tokstart++; // exclude '#'
  // advance s->inp until next <LF> or EOF. Leave s->inp at \n or EOF.
  while (s->inp < s->inend && *s->inp != '\n') {
    s->inp++;
  }
  s->tokend = s->inp;
  if (s->flags & SCAN_COMMENTS) {
    addComment(s);
  }
}


// read unicode name
static void snameuni(S* s) {
  while (s->inp < s->inend) {
    u8 b = *s->inp;
    Rune r;
    if (b < RuneSelf) {
      if (!(charflags[b] & CH_IDENT)) {
        break;
      }
      r = b;
      s->inp++;
    } else {
      u32 w;
      r = utf8decode(s->inp, s->inend - s->inp, &w);
      s->inp += w;
      if (r == RuneErr) {
        serr(s, "invalid UTF-8 encoding");
      }
    }
    if (r == 0) {
      serr(s, "invalid NUL character");
    }
  }
  s->tokend = s->inp;
  s->name = symgeth(s->tokstart, s->tokend - s->tokstart);
  s->tok = symLangTok(s->name);
}


// read ASCII name (may switch over to snameuni)
static void sname(S* s) {
  // names are pre-hashed and then converted into interned Sym objects.
  const u32 prime = 0x01000193; // FNV1a prime
  u32 hash = 0x811C9DC5; // FNV1a seed
  hash = (*(s->inp-1) ^ hash) * prime; // hash first byte (sname called after first byte)

  while (s->inp < s->inend && charflags[*s->inp] & CH_IDENT) {
    hash = (*s->inp ^ hash) * prime;
    s->inp++;
  }

  if (*s->inp >= RuneSelf && s->inp < s->inend) {
    // s->inp = s->tokstart;
    return snameuni(s);
  }

  s->tokend = s->inp;
  s->name = symget(s->tokstart, s->tokend - s->tokstart, hash);
  s->tok = symLangTok(s->name);

  // dlog("got name %s \t%p\t%s\thash=%u", sdscatrepr(sdsempty(), s->name, symlen(s->name)),
  //   s->name, TokName(s->tok), hash);
}


// scan a number
void snumber(S* s) {
  while (s->inp < s->inend) {
    u8 c = *s->inp;
    if (c > '9' || c < '0') {
      break;
    }
    s->inp++;
  }
  s->tokend = s->inp; // XXX
  s->tok = TIntLit;
}


Tok SNext(S* s) {
  scan_again:  // jumped to when comments are skipped

  // skip whitespace
  while (s->inp < s->inend && charflags[*s->inp] & CH_WHITESPACE) {
    if (*s->inp == '\n') {
      s->lineno++;
      s->linestart = s->inp;
      if (s->insertSemi) {
        s->insertSemi = false;
        s->tokstart = s->inp;
        s->tokend = s->tokstart;
        s->inp++;
        return s->tok = TSemi;
      }
    }
    s->inp++;
  }

  // EOF
  if (s->inp == s->inend) {
    s->tokstart = s->inp - 1;
    s->tokend = s->tokstart;
    if (s->insertSemi) {
      s->insertSemi = false;
      s->tok = TSemi;
    } else {
      s->tok = TNil;
    }
    return s->tok;
  }

  s->tokstart = s->inp;
  s->tokend = s->tokstart + 1;

  bool insertSemi = false;

  u8 c = *s->inp++;
  switch (c) {

  case '+':
  case '-':
    if (s->inp < s->inend) {
      if (*s->inp == c) {
        s->inp++;
        s->tok = c == '+' ? TPlusPlus : TMinusMinus;
        s->tokend++;
        insertSemi = true;
        break;
      } else if (c == '-' && *s->inp == '>') {
        s->inp++;
        s->tok = TRArr;
        s->tokend++;
        break;
      }
    }
    s->tok = (Tok)c;
    break;

  case '!':
  case '=':
    if (s->inp+1 < s->inend && *s->inp == '=') {
      s->inp++;
      s->tok = c == '=' ? TEqEq : TNEq;
      s->tokend++;
      break;
    }
    s->tok = (Tok)c;
    break;

  case ')':
  case '}':
  case ']':
    insertSemi = true;
    FALLTHROUGH;
  case '(':
  case '{':
  case '[':
  case '~':
  case '*':
  case '/':
  case ',':
  case ';':
  case '>':
  case '<':
    s->tok = (Tok)c;
    break;

  case '#': // line comment
    scomment(s);
    goto scan_again;
    break;

  case '0'...'9':
    snumber(s);
    insertSemi = true;
    break;

  case '$':
  case '_':
  case 'A'...'Z':
  case 'a'...'z':
    sname(s);
    switch (s->tok) {
      case TIdent:
      case TBreak:
      case TContinue:
      case TFallthrough:
      case TReturn:
        insertSemi = true;
        break;
      default:
        break;
    }
    break;

  default:
    if (c >= RuneSelf) {
      s->inp--;
      snameuni(s);
      insertSemi = true;
      break;
    }
    // invariant: c < RuneSelf
    s->tokend = s->tokstart;
    s->tok = TNil;
    if (c >= 0x20 && c < 0x7F) {
      serr(s, "invalid input character '%C' 0x%x", c, c);
    } else {
      serr(s, "invalid input character 0x%x", c);
    }
    break;

  } // switch

  s->insertSemi = insertSemi;


  #ifdef SCANNER_DEBUG_TOKEN_PRODUCTION
  {
    auto srcpos = SrcPosFmt(sdsempty(), SSrcPos(s));
    if (s->tok < TSymbolicStart) {
      dlog(">> %s\t\tat %s", TokName(s->tok), srcpos);
    } else {
      dlog(">> %s \"%.*s\"\tat %s",
        TokName(s->tok), (int)(s->tokend - s->tokstart), s->tokstart, srcpos);
    }
  }
  #endif


  return s->tok;
}




/*static Rune nextr(S* s) {
  if (s->inp >= s->inend) {
    return -1;  // EOF
  }

  s->inp0 = s->inp;  // save current position

  u8 b;

  // common case: ASCII and enough bytes
  redo:
  b = *s->inp++;
  if (b < RuneSelf) {
    if (b == 0) {
      serr(s, "invalid NUL character");
      goto redo;
    }
    if (b == '\n') {
      s->lineno++;
      s->linestart = s->inp; // note: inp at first byte AFTER <LF>
    }
    return (Rune)b;
  }

  u32 w;
  auto u = utf8decode(s->inp, s->inend - s->inp, &w);
  dlog("utf8decode => 0x%08X", u);

  return RuneErr;

  // if (decodeRes.c >= 0x80) {
  //   // uncommon case: non-ASCII character
  //   if (!utf8.decode(s.sdata, s.rdOffset, decodeRes)) {
  //     s.errorAtOffs('invalid UTF-8 encoding', s.offset)
  //   } else if (decodeRes.c == 0) {
  //     s.errorAtOffs('illegal NUL byte in input', s.offset)
  //   }
  // }
}*/