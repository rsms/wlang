#pragma once
#include "defs.h"

u32 hashFNV1a(const u8* buf, size_t len);
u64 hashFNV1a64(const u8* buf, size_t len);
