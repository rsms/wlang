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
/*END W_TYPE_SYMS*/


// TypeCode identifies all basic types
typedef enum {
  // named types exported in the global scope. Names must match those of TYPE_SYMS.
  TypeCode_bool    = 'b',
  TypeCode_int     = 'i',
  TypeCode_uint    = 'u',
  TypeCode_int8    = '1',
  TypeCode_int16   = '2',
  TypeCode_int32   = '4',
  TypeCode_int64   = '8',
  TypeCode_uint8   = '3',
  TypeCode_uint16  = '5',
  TypeCode_uint32  = '6',
  TypeCode_uint64  = '7',
  TypeCode_float32 = 'f',
  TypeCode_float64 = 'F',
  TypeCode_str     = 's',

  // internal types not directly reachable by names in the language
  TypeCode_nil     = '0',
  TypeCode_fun     = '^',
  TypeCode_tuple   = '(', TypeCode_tupleEnd  = ')',
  TypeCode_list    = '[', TypeCode_listEnd   = '[',
  TypeCode_struct  = '{', TypeCode_structEnd = '{',
} TypeCode;

