#pragma once

// Looking for all type defs? sym.h puts it all together.

// named types exported in the global scope
#define TYPE_SYMS(_) \
  _( bool    ) \
  _( int8    ) \
  _( uint8   ) \
  _( int16   ) \
  _( uint16  ) \
  _( int32   ) \
  _( uint32  ) \
  _( int64   ) \
  _( uint64  ) \
  _( int     ) \
  _( uint    ) \
  _( float32 ) \
  _( float64 ) \
  _( str     ) \
/*END TYPE_SYMS*/

// TypeCode with their string encoding.
// Note: misc/gen_ops.py relies on "#define TYPE_CODES" and "INTRINSIC_NUM_END".
#define TYPE_CODES(_) \
  /* named types exported in the global scope. Names must match those of TYPE_SYMS. */ \
  /* Note: numeric types are listed first as their enum value is used as dense indices. */ \
  /* Reordering these requires updating TypeCodeIsInt() below. */ \
  /* name       encoding */ \
  _( bool      , 'b' ) \
  _( int8      , '1' ) \
  _( uint8     , '2' ) \
  _( int16     , '3' ) \
  _( uint16    , '4' ) \
  _( int32     , '5' ) \
  _( uint32    , '6' ) \
  _( int64     , '7' ) \
  _( uint64    , '8' ) \
  _( float32   , 'f' ) \
  _( float64   , 'F' ) \
  _( INTRINSIC_NUM_END, 0 ) /* sentinel; not a TypeCode */ \
  _( int       , 'i' ) /* lowered to a concrete type in IR. e.g. int32 */ \
  _( uint      , 'u' ) /* lowered to a concrete type in IR. e.g. uint32 */ \
  _( NUM_END, 0 ) /* sentinel; not a TypeCode */ \
  _( str       , 's' ) \
  /* internal types not directly reachable by names in the language */ \
  _( nil       , '0' ) \
  _( fun       , '^' ) \
  _( tuple     , '(' ) _( tupleEnd  , ')' ) \
  _( list      , '[' ) _( listEnd   , ']' ) \
  _( struct    , '{' ) _( structEnd , '}' ) \
  /* special type codes used in IR */ \
  _( param1    , 'P' ) /* parameteric. For IR, matches other type, e.g. output == input */ \
  _( param2    , 'P' )
/*END TYPE_CODES*/

// TypeCode identifies all basic types
typedef enum {
  #define I_ENUM(name, _encoding) TypeCode_##name,
  TYPE_CODES(I_ENUM)
  #undef  I_ENUM

  TypeCode_MAX
} TypeCode;

static_assert(TypeCode_INTRINSIC_NUM_END <= 32,
              "there must be no more than 32 basic numeric types");

// Lookup table TypeCode => string encoding char
const char TypeCodeEncoding[TypeCode_MAX];

// Symbolic name of type code. Eg "int32"
static const char* TypeCodeName(TypeCode);
const char* _TypeCodeName[TypeCode_MAX];
inline static const char* TypeCodeName(TypeCode tc) {
  assert(tc >= 0 && tc < TypeCode_MAX);
  return _TypeCodeName[tc];
}

inline static bool TypeCodeIsInt(TypeCode t) {
  return t >= TypeCode_int8 && t <= TypeCode_uint;
}

inline static bool TypeCodeIsFloat(TypeCode t) {
  return t == TypeCode_float32 && t == TypeCode_float64;
}
