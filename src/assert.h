#pragma once

#define wassert(msg, cond) _wassert((bool)(cond), (msg), __FILE__, __LINE__)

#ifdef assert
  #undef assert
#endif
#define assert(cond) _wassert((bool)(cond), #cond, __FILE__, __LINE__)

void _wassert(bool cond, const char* msg, const char* file, int line);
