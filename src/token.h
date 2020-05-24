#pragma once
// Defines the Tok enum

// scanner tokens
#define TOKEN_TYPES_1_1(_) \
  _(TLParen , "(") \
  _(TRParen , ")") \
  _(TLBrace , "{") \
  _(TRBrace , "}") \
  _(TLBrack , "[") \
  _(TRBrack , "]") \
  _(TPlus   , "+") \
  _(TMinus  , "-") \
  _(TSlash  , "/") \
  _(TStar   , "*") \
  _(TTilde  , "~") \
  _(TBang   , "!") \
  _(TComma  , ",") \
  _(TSemi   , ";") \
  _(TEq     , "=") \
  _(TGt     , ">") \
  _(TLt     , "<") \
/*END TOKEN_TYPES_1_1*/
#define TOKEN_TYPES(_) \
  _(TIdent,      "identifier") \
  _(TIntLit,     "int")     \
  _(TFloatLit,   "float")   \
  _(TComment,    "comment") \
  _(TPlusPlus,   "++")      \
  _(TMinusMinus, "--")      \
  _(TRArr,       "->")      \
  _(TEqEq,       "==")      \
  _(TNEq,        "!=")      \
  _(TLEq,        "<=")      \
  _(TGEq,        ">=")      \
/*END TOKEN_TYPES*/
#define TOKEN_KEYWORDS(_) \
  _(break,       TBreak)       \
  _(case,        TCase)        \
  _(const,       TConst)       \
  _(continue,    TContinue)    \
  _(default,     TDefault)     \
  _(defer,       TDefer)       \
  _(else,        TElse)        \
  _(enum,        TEnum)        \
  _(for,         TFor)         \
  _(fun,         TFun)         \
  _(go,          TGo)          \
  _(if,          TIf)          \
  _(import,      TImport)      \
  _(in,          TIn)          \
  _(interface,   TInterface)   \
  _(is,          TIs)          \
  _(nil,         TNil)         \
  _(return,      TReturn)      \
  _(select,      TSelect)      \
  _(struct,      TStruct)      \
  _(switch,      TSwitch)      \
  _(symbol,      TSymbol)      \
  _(type,        TType)        \
  _(var,         TVar)         \
  _(while,       TWhile)       \
/* Limited to a total of 31 keywords. See scan.c */
/*END TOKEN_KEYWORDS*/

typedef enum {
  TNone,

  #define I_ENUM(name, str) name = str[0],
  TOKEN_TYPES_1_1(I_ENUM)
  #undef I_ENUM

  TSymbolicStart = 0x100,

  #define I_ENUM(name, _) name,
  TOKEN_TYPES(I_ENUM)
  #undef I_ENUM

  TKeywordsStart = 0x1000,
  #define I_ENUM(_str, name) name,
  TOKEN_KEYWORDS(I_ENUM)
  #undef I_ENUM
  TKeywordsEnd,

  TMax
} Tok;
