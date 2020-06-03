#pragma once
// Defines the Tok enum

// scanner tokens
#define TOKENS(_) \
  _( TComma      , ",")  \
  _( TSemi       , ";")  \
  _( TEq         , "=")  \
  _( T_OPS_START , "") /* start of operator tokens */ \
  _( TPlus       , "+")  \
  _( TMinus      , "-")  \
  _( TStar       , "*")  \
  _( TSlash      , "/")  \
  _( TGt         , ">")  \
  _( TLt         , "<")  \
  _( TEqEq       , "==") \
  _( TNEq        , "!=") \
  _( TLEq        , "<=") \
  _( TGEq        , ">=") \
  _( TPlusPlus   , "++") \
  _( TMinusMinus , "--") \
  _( TTilde      , "~")  \
  _( TBang       , "!")  \
  _( T_OPS_END   , "") /* end of operator tokens */ \
  _( TLParen     , "(") \
  _( TRParen     , ")") \
  _( TLBrace     , "{") \
  _( TRBrace     , "}") \
  _( TLBrack     , "[") \
  _( TRBrack     , "]") \
  _( TRArr       , "->") \
  _( TIdent      , "identifier") \
  _( TIntLit     , "int")     \
  _( TFloatLit   , "float")   \
  _( TComment    , "comment") \
/*END TOKENS*/
#define TOKEN_KEYWORDS(_) \
  _( as,          TAs)          \
  _( break,       TBreak)       \
  _( case,        TCase)        \
  _( continue,    TContinue)    \
  _( default,     TDefault)     \
  _( defer,       TDefer)       \
  _( else,        TElse)        \
  _( enum,        TEnum)        \
  _( for,         TFor)         \
  _( fun,         TFun)         \
  _( if,          TIf)          \
  _( import,      TImport)      \
  _( in,          TIn)          \
  _( interface,   TInterface)   \
  _( is,          TIs)          \
  _( mutable,     TMutable)     \
  _( nil,         TNil)         \
  _( return,      TReturn)      \
  _( select,      TSelect)      \
  _( struct,      TStruct)      \
  _( switch,      TSwitch)      \
  _( symbol,      TSymbol)      \
  _( type,        TType)        \
  _( while,       TWhile)       \
// Limited to a total of 31 keywords. See scan.c
//END TOKEN_KEYWORDS

typedef enum {
  TNone,

  #define I_ENUM(name, str) name,
  TOKENS(I_ENUM)
  #undef I_ENUM

  // TKeywordsStart is used for 0-based keyword indexing.
  // It's explicit value is used by sym.c to avoid having to regenerate keyword symbols
  // whenever a non-keyword token is added. I.e. this number can be changed freely but will
  // require regeneration of the code in sym.c.
  TKeywordsStart = 0x100,
  #define I_ENUM(_str, name) name,
  TOKEN_KEYWORDS(I_ENUM)
  #undef I_ENUM
  TKeywordsEnd,

  TMax
} Tok;

static_assert(TKeywordsEnd - TKeywordsStart <= 32, "too many keywords");

// Get printable name
const char* TokName(Tok);
