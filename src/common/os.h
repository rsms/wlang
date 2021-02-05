#pragma once
#include "memory.h"

// os
size_t os_mempagesize();  // always returns a suitable number

// Read entire file into a heap-allocated buffer.
// If *size_inout is >0 then it is used as a limit of how much to read from the file.
// If size_inout is not null, it is set to the size of the returned byte array.
u8* os_readfile(const char* nonull filename, size_t* nonull size_inout, Memory nullable mem);

// Write data at ptr of bytes size to file at filename.
bool os_writefile(const char* nonull filename, const void* nonull ptr, size_t size);
