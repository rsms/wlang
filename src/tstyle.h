#pragma once

typedef enum {
  TStyle_bold,         // : sfn('1', '1', '22'),
  TStyle_italic,       // : sfn('3', '3', '23'),
  TStyle_underline,    // : sfn('4', '4', '24'),
  TStyle_inverse,      // : sfn('7', '7', '27'),
  TStyle_white,        // : sfn('37', '38;2;255;255;255', '39'),
  TStyle_grey,         // : sfn('90', '38;5;244', '39'),
  TStyle_black,        // : sfn('30', '38;5;16', '39'),
  TStyle_blue,         // : sfn('34', '38;5;75', '39'),
  TStyle_cyan,         // : sfn('36', '38;5;87', '39'),
  TStyle_green,        // : sfn('32', '38;5;84', '39'),
  TStyle_magenta,      // : sfn('35', '38;5;213', '39'),
  TStyle_purple,       // : sfn('35', '38;5;141', '39'),
  TStyle_pink,         // : sfn('35', '38;5;211', '39'),
  TStyle_red,          // : sfn('31', '38;2;255;110;80', '39'),
  TStyle_yellow,       // : sfn('33', '38;5;227', '39'),
  TStyle_lightyellow,  // : sfn('93', '38;5;229', '39'),
  TStyle_orange,       // : sfn('33', '38;5;215', '39'),
  _TStyle_MAX,
} TStyle;

const char* TStyleTable[_TStyle_MAX];
const char* TStyle_none;

static inline Str TStyleBold(Str s) { return sdscat(s, TStyleTable[TStyle_bold]); }
static inline Str TStyleItalic(Str s) { return sdscat(s, TStyleTable[TStyle_italic]); }
static inline Str TStyleUnderline(Str s) { return sdscat(s, TStyleTable[TStyle_underline]); }
static inline Str TStyleInverse(Str s) { return sdscat(s, TStyleTable[TStyle_inverse]); }
static inline Str TStyleWhite(Str s) { return sdscat(s, TStyleTable[TStyle_white]); }
static inline Str TStyleGrey(Str s) { return sdscat(s, TStyleTable[TStyle_grey]); }
static inline Str TStyleBlack(Str s) { return sdscat(s, TStyleTable[TStyle_black]); }
static inline Str TStyleBlue(Str s) { return sdscat(s, TStyleTable[TStyle_blue]); }
static inline Str TStyleCyan(Str s) { return sdscat(s, TStyleTable[TStyle_cyan]); }
static inline Str TStyleGreen(Str s) { return sdscat(s, TStyleTable[TStyle_green]); }
static inline Str TStyleMagenta(Str s) { return sdscat(s, TStyleTable[TStyle_magenta]); }
static inline Str TStylePurple(Str s) { return sdscat(s, TStyleTable[TStyle_purple]); }
static inline Str TStylePink(Str s) { return sdscat(s, TStyleTable[TStyle_pink]); }
static inline Str TStyleRed(Str s) { return sdscat(s, TStyleTable[TStyle_red]); }
static inline Str TStyleYellow(Str s) { return sdscat(s, TStyleTable[TStyle_yellow]); }
static inline Str TStyleLightyellow(Str s) { return sdscat(s, TStyleTable[TStyle_lightyellow]); }
static inline Str TStyleOrange(Str s) { return sdscat(s, TStyleTable[TStyle_orange]); }

static inline Str TStyleNone(Str s) { return sdscat(s, TStyle_none); }
