#include "wp.h"
#include "types.h"

// Lookup table TypeCode => string encoding char
const char TypeCodeEncoding[TypeCode_MAX] = {
  #define I_ENUM(name, encoding, _flags) encoding,
  TYPE_CODES(I_ENUM)
  #undef  I_ENUM
};


// #if DEBUG
const char* _TypeCodeName[TypeCode_MAX] = {
  #define I_ENUM(name, _encoding, _flags) #name,
  TYPE_CODES(I_ENUM)
  #undef  I_ENUM
};


const TypeCodeFlag TypeCodeFlagMap[TypeCode_MAX] = {
  #define I_ENUM(_name, _encoding, flags) flags,
  TYPE_CODES(I_ENUM)
  #undef  I_ENUM
};

// const char* TypeCodeName(TypeCode tc) {
//   assert(tc > 0 && tc < TypeCode_MAX);
//   return _TypeCodeName[tc];
// }
// #else
//   // compact names where a string is formed from encoding chars + sentinels bytes.
//   // E.g. "b\01\02\03\04\05\06\07\08\0f\0F\0..." Index is *2 that of TypeCode.
//   static const char _TypeCodeName[TypeCode_MAX * 2] = {
//     #define I_ENUM(_, enc) enc, 0,
//     TYPE_CODES(I_ENUM)
//     #undef  I_ENUM
//   };
//   const char* TypeCodeName(TypeCode tc) {
//     assert(tc > 0 && tc < TypeCode_MAX);
//     return &_TypeCodeName[tc * 2];
//   }
// #endif
