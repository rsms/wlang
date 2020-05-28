#pragma once

// Looking for all type defs? sym.h puts it all together.

// intrinsic types, exported in the global scope
#define TYPE_SYMS(_) \
  _( bool    ) \
  _( int     ) \
  _( uint    ) \
  _( int8    ) \
  _( int16   ) \
  _( int32   ) \
  _( int64   ) \
  _( uint8   ) \
  _( uint16  ) \
  _( uint32  ) \
  _( uint64  ) \
  _( float32 ) \
  _( float64 ) \
  _( str     ) \
/*END TYPE_SYMS*/

// TypeCode with their string encoding
#define TYPE_CODES(_) \
  /* named types exported in the global scope. Names must match those of TYPE_SYMS. */ \
  /* Note: numeric types are listed first as their enum value is used as dense indices. */ \
  _( int       , 'i' ) \
  _( uint      , 'u' ) \
  _( int8      , '1' ) \
  _( int16     , '2' ) \
  _( int32     , '4' ) \
  _( int64     , '8' ) \
  _( uint8     , '3' ) \
  _( uint16    , '5' ) \
  _( uint32    , '6' ) \
  _( uint64    , '7' ) \
  _( float32   , 'f' ) \
  _( float64   , 'F' ) \
  _( NUM_END   , '\0' ) /* sentinel; not a TypeCode */ \
  _( bool      , 'b' ) \
  _( str       , 's' ) \
  /* internal types not directly reachable by names in the language */ \
  _( nil       , '0' ) \
  _( fun       , '^' ) \
  _( tuple     , '(' ) _( tupleEnd  , ')' ) \
  _( list      , '[' ) _( listEnd   , ']' ) \
  _( struct    , '{' ) _( structEnd , '}' ) \
/*END TYPE_CODES*/


// TypeCode identifies all basic types
typedef enum {
  #define I_ENUM(name, _encoding) TypeCode_##name,
  TYPE_CODES(I_ENUM)
  #undef  I_ENUM

  TypeCode_MAX
} TypeCode;

static_assert(TypeCode_NUM_END <= 32, "there must be no more than 32 basic numeric types");

// Lookup table TypeCode => string encoding char
const char TypeCodeEncoding[TypeCode_MAX];
