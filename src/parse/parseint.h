#pragma once
#include "../common/defs.h"

bool parseint32(const char* ptr, size_t len, int base, u32* result);
bool parseint64(const char* ptr, size_t len, int base, u64* result);
