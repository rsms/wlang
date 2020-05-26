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


// TypeID identifies all basic types
typedef enum {
  // named types exported in the global scope
  TypeID_bool    = 'b',
  TypeID_int     = 'i',
  TypeID_uint    = 'u',
  TypeID_int8    = '1',
  TypeID_int16   = '2',
  TypeID_int32   = '4',
  TypeID_int64   = '8',
  TypeID_uint8   = '3',
  TypeID_uint16  = '5',
  TypeID_uint32  = '6',
  TypeID_uint64  = '7',
  TypeID_float32 = 'f',
  TypeID_float64 = 'F',
  TypeID_str     = 's',

  // internal types not directly reachable by names in the language
  TypeID_nil     = '0',
  TypeID_fun     = '^',
  TypeID_tuple   = '(', TypeID_tupleEnd  = ')',
  TypeID_list    = '[', TypeID_listEnd   = '[',
  TypeID_struct  = '{', TypeID_structEnd = '{',
} TypeID;
