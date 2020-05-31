#include "wp.h"
#include "types.h"

// Lookup table TypeCode => string encoding char
const char TypeCodeEncoding[TypeCode_MAX] = {
  #define I_ENUM(name, encoding) encoding,
  TYPE_CODES(I_ENUM)
  #undef  I_ENUM
};
