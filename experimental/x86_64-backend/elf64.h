#pragma once
#include "../defs.h"
#include "../buf.h"

typedef struct ELF64 {
  Buf    buf;        // Main buffer (ELF header + program headers + data segments)
  u16    phnum;      // number of program headers (in buf + sizeof(ELH header))
  Buf    shbuf;      // section headers
  Buf    strtab;     // string table
  Buf    shstrtab;   // section header string table
  Buf    symtab;     // symbol table
} ELF64;

void ELF64Init(ELF64* e, Memory nullable mem);
void ELF64Free(ELF64* e);

inline static Memory* ELF64Memory(ELF64* e) {
  return e->buf.mem;
}

inline static Elf64_Ehdr* ELF64GetEH(ELF64* e) {
  return (Elf64_Ehdr*)e->buf.ptr;
}
