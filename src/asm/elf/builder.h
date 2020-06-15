#pragma once
#include "../../array.h"
#include "../../buf.h"

typedef struct ELFData    ELFData;
typedef struct ELFSec     ELFSec;
typedef struct ELFProg    ELFProg;
typedef struct ELFBuilder ELFBuilder;

typedef enum ELFErr {
  ELF_OK = 0,
  ELF_E_UNSPECIFIED,
} ELFErr;

typedef enum ELFMode {
  ELFMode32,
  ELFMode64,
} ELFMode;

// Section header
typedef struct ELFSec {
  ELFBuilder* builder; // owning builder
  ELFData*    data;    // section data pointer. May be NULL.
  u16         index;   // section index
  // ELF type-agnostic members of Elf32_Shdr & Elf64_Shdr
  u32         type;    // Type of section (sh_type)
  u32         name;    // Section name, index in shstrtab (sh_name)
  u32         flags;   // Bitflags ELF_SHF_* (sh_flags)
  u32         link;    // Index of another section (sh_link)
  // Data used during assembly
  union {
    Elf32_Shdr sh32;
    Elf64_Shdr sh64;
  };
} ELFSec;

// Program header
typedef struct ELFProg {
  ELFBuilder* builder; // owning builder
  ELFData*    data;    // segment data pointer. May be NULL.
  // ELF type-agnostic members of Elf32_Phdr & Elf64_Phdr
  u32         type;    // (p_type)
  u32         flags;   // (p_flags)
  // Data used during assembly
  union {
    u32 align32;
    u64 align64;
  };
} ELFProg;

// ELFData represents a segment and/or section data.
// Referenced by at least one of either a section header or a program header (or both.)
typedef struct ELFData {
  ELFBuilder* builder;
  Array       secv;  // [ELFSec*] section headers referencing this data
  void*       _secv_storage[1];
  Array       progv; // [ELFProg*] program headers referencing this data
  void*       _progv_storage[1];
  Buf         buf;   // the data
  // Data used during assembly
  union {
    u32 offs32;
    u64 offs64;
  };
} ELFData;

// Builder
typedef struct ELFBuilder {
  Memory       mem;      // allocator (NULL = global allocator)
  ELFMode      mode;
  ELFMachine   machine;
  Array        dv;       // data segments [ELFData*]
  Array        shv;      // section headers [ELFSec*]
  Array        phv;      // program headers [ELFProg*]
  // special sections (pointers into shv)
  ELFSec*      shstrtab; // ".shstrtab" Section Header string table section
  ELFSec*      strtab;   // ".strtab" General string table section
  ELFSec*      symtab;   // ".symtab" General symbol table section
} ELFBuilder;


// Initialize a builder for use.
void ELFBuilderInit(ELFBuilder* b, ELFMachine m, Memory nullable mem);

// Free all memory used by the builder (does not free memory for b itself.)
void ELFBuilderFree(ELFBuilder* b);

// Allocate a new data to be linked with a section and/or program header.
ELFData* ELFBuilderNewData(ELFBuilder* b);

// Add a new section header of type with name which optionally references data.
ELFSec* ELFBuilderNewSec(ELFBuilder* b, const char* name, u32 type, ELFData* data);

// Add a new program header of type with name which optionally references data.
ELFProg* ELFBuilderNewProg(ELFBuilder* b, u32 type, u32 flags, ELFData* data);

// Add a new SYMTAB section named name, which stores its names in strtab.
ELFSec* ELFBuilderNewSymtab(ELFBuilder* b, const ELFSec* strtab, const char* name);

// Retrieves the null-terminated name of the section, as provided to ELFBuilderNewSec.
const char* ELFSecName(const ELFSec* sec);

// Append a name to a string table. Return its index. strtab->type must be STRTAB.
u32 ELFStrtabAppend(ELFSec* strtab, const char* name);

// Look up a name in a string table. nameindex is a byte offset.
const char* ELFStrtabLookup(const ELFSec* sec, u32 nameindex);

// Add a symbol with name to symtab, originating in section with index shndx.
// Returns a pointer to the symbol.
// The returned pointer is only valid until the next call to ELFSymtabAdd* as it
// references memory that might change during a call.
Elf32_Sym* ELFSymtabAdd32(ELFSec* symtab, u16 shndx, const char* name, u8 bind, u8 typ, u32 value);
Elf64_Sym* ELFSymtabAdd64(ELFSec* symtab, u16 shndx, const char* name, u8 bind, u8 typ, u64 value);

// Assemble ELF file
ELFErr ELFBuilderAssemble(ELFBuilder* b, Buf* buf);
