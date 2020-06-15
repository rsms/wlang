#pragma once
//
// This file provides definitions for ELF - Executable and Linkable Format
// as the the Portable Formats Specification, Version 1.2.
//

// ---------------------------------------------------------------------------------------------
// ELF data types
typedef u64 Elf64_Addr;   // Unsigned program address
typedef u16 Elf64_Half;   // Unsigned medium integer
typedef i16 Elf64_SHalf;  // Signed medium integer
typedef u64 Elf64_Off;    // Unsigned file offset
typedef u32 Elf64_Word;   // Unsigned large integer
typedef i32 Elf64_Sword;  // Signed large integer
typedef u64 Elf64_Xword;  // Unsigned eXtra large integer
typedef i64 Elf64_Sxword; // Signed eXtra large integer

typedef u32 Elf32_Addr;
typedef u16 Elf32_Half;
typedef u32 Elf32_Off;
typedef i32 Elf32_Sword;
typedef u32 Elf32_Word;

// ---------------------------------------------------------------------------------------------
// Elf64_Ehdr
#define ELF_EI_NIDENT   16
#define ELF_V_NONE      0
#define ELF_V_CURRENT   1
#define ELF_OSABI_NONE  0
#define ELF_OSABI_LINUX 3

typedef struct elf64_hdr {
  unsigned char e_ident[ELF_EI_NIDENT]; // ELF "magic" & identity
  Elf64_Half    e_type;
  Elf64_Half    e_machine;
  Elf64_Word    e_version;
  Elf64_Addr    e_entry;    // Entry point virtual address
  Elf64_Off     e_phoff;    // Program header table file offset
  Elf64_Off     e_shoff;    // Section header table file offset
  Elf64_Word    e_flags;
  Elf64_Half    e_ehsize;
  Elf64_Half    e_phentsize;
  Elf64_Half    e_phnum;
  Elf64_Half    e_shentsize;
  Elf64_Half    e_shnum;
  Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

typedef struct elf32_hdr {
  unsigned char e_ident[ELF_EI_NIDENT];
  Elf32_Half  e_type;
  Elf32_Half  e_machine;
  Elf32_Word  e_version;
  Elf32_Addr  e_entry;
  Elf32_Off   e_phoff;
  Elf32_Off   e_shoff;
  Elf32_Word  e_flags;
  Elf32_Half  e_ehsize;
  Elf32_Half  e_phentsize;
  Elf32_Half  e_phnum;
  Elf32_Half  e_shentsize;
  Elf32_Half  e_shnum;
  Elf32_Half  e_shstrndx;
} Elf32_Ehdr;

// ---------------------------------------------------------------------------------------------
// Elf64_Phdr
typedef struct elf64_phdr {
  Elf64_Word  p_type;
  Elf64_Word  p_flags;  // See ELF_PF_*
  Elf64_Off   p_offset; // Segment file offset
  Elf64_Addr  p_vaddr;  // Segment virtual address
  Elf64_Addr  p_paddr;  // Segment physical address
  Elf64_Xword p_filesz; // Segment size in file
  Elf64_Xword p_memsz;  // Segment size in memory
  Elf64_Xword p_align;  // Segment alignment, file & memory
} Elf64_Phdr;

typedef struct elf32_phdr{
  Elf32_Word p_type;
  Elf32_Off  p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;

// ---------------------------------------------------------------------------------------------
// Elf64_Shdr
typedef struct elf64_shdr {
  Elf64_Word  sh_name;      // Section name, index in string tbl
  // This member specifies the name of the section. Its value is an index into the
  // section header string table section, giving the location of a null-terminated string.
  //
  Elf64_Word  sh_type;      // Type of section. See ELF_SHT_*
  // This member categorizes the section's contents and semantics.
  //
  Elf64_Xword sh_flags;     // Miscellaneous section attributes. See ELF_SHF_*
  // Sections support 1-bit flags that describe miscellaneous attributes.
  //
  Elf64_Addr  sh_addr;      // Section virtual addr at execution
  // If the section will appear in the memory image of a process, this member gives
  // the address at which the section's first byte should reside. Otherwise, the
  // member contains 0.
  //
  Elf64_Off   sh_offset;    // Section's data file offset
  // This member's value gives the byte offset from the beginning of the file to the
  // first byte in the section. One section type, SHT_NOBITS, occupies no space in the file,
  // and its sh_offset member locates the conceptual placement in the file.
  //
  Elf64_Xword sh_size;      // Size of section in bytes
  // This member gives the section's size in bytes. Unless the section type is
  // SHT_NOBITS, the section occupies sh_size bytes in the file. A section of type
  // SHT_NOBITS may have a non-zero size, but it occupies no space in the file.
  //
  Elf64_Word  sh_link;      // Index of another section
  // This member holds a section header table index link, whose interpretation
  // depends on the section type.
  //
  Elf64_Word  sh_info;      // Additional section information
  // This member holds extra information, whose interpretation depends on the section type.
  // If the sh_flags field for this section header includes the attribute SHF_INFO_LINK,
  // then this member represents a section header table index.
  //
  Elf64_Xword sh_addralign; // Section alignment
  // Some sections have address alignment constraints. For example, if a section
  // holds a doubleword, the system must ensure doubleword alignment for the entire
  // section. The value of sh_addr must be congruent to 0, modulo the value of
  // sh_addralign. Currently, only 0 and positive integral powers of two are allowed.
  // Values 0 and 1 mean the section has no alignment constraints.
  //
  Elf64_Xword sh_entsize;   // Entry size if section holds table
  // Some sections hold a table of fixed-size entries, such as a symbol table. For
  // such a section, this member gives the size in bytes of each entry. The member
  // contains 0 if the section does not hold a table of fixed-size entries.
} Elf64_Shdr;

typedef struct elf32_shdr {
  Elf32_Word  sh_name;
  Elf32_Word  sh_type;
  Elf32_Word  sh_flags;
  Elf32_Addr  sh_addr;
  Elf32_Off   sh_offset;
  Elf32_Word  sh_size;
  Elf32_Word  sh_link;
  Elf32_Word  sh_info;
  Elf32_Word  sh_addralign;
  Elf32_Word  sh_entsize;
} Elf32_Shdr;

// ---------------------------------------------------------------------------------------------
// Elf64_Sym
// An object file's symbol table holds information needed to locate and relocate a program's
// symbolic definitions and references. A symbol table index is a subscript into this array.
// Index 0 both designates the first entry in the table and serves as the undefined symbol index.
// The contents of the initial entry are specified later in this section.
typedef struct elf64_sym {
  Elf64_Word    st_name;  // Symbol name, index in string tbl
  // This member holds an index into the object file's symbol string table, which holds
  // the character representations of the symbol names. If the value is non-zero, it
  // represents a string table index that gives the symbol name. Otherwise, the symbol table
  // entry has no name.
  //
  unsigned char st_info;  // Type and binding attributes (ELF_STB_* and ELF_STT_*)
  // This member specifies the symbol's type and binding attributes.
  //
  unsigned char st_other; // No defined meaning, 0. (used for visibility in ELF32)
  //
  Elf64_Half    st_shndx; // Associated section index (section sym "comes" from or "belongs" to)
  // Every symbol table entry is defined in relation to some section. This member
  // holds the relevant section header table index. As the sh_link and sh_info
  // interpretation table and the related text describe, some section indexes
  // indicate special meanings.
  //   If this member contains SHN_XINDEX, then the actual section header index is too
  // large to fit in this field. The actual value is contained in the associated
  // section of type SHT_SYMTAB_SHNDX.
  //
  Elf64_Addr    st_value; // Value of the symbol
  // This member gives the value of the associated symbol. Depending on the context, this
  // may be an absolute value, an address, and so on.
  // - In executable and shared object files, st_value holds a virtual address.
  //   To make these files' symbols more useful for the dynamic linker, the section offset
  //   (file interpretation) gives way to a virtual address (memory interpretation) for which
  //   the section number is irrelevant.
  // - In relocatable files, st_value holds alignment constraints for a symbol whose section
  //   index is SHN_COMMON
  // - In relocatable files, st_value holds a section offset for a defined symbol.
  //   st_value is an offset from the beginning of the section that st_shndx identifies.
  //
  Elf64_Xword   st_size;  // Associated symbol size
  // Many symbols have associated sizes. For example, a data object's size is the number of
  // bytes contained in the object. This member holds 0 if the symbol has no size or an
  // unknown size.
} Elf64_Sym;  // sizeof(Elf64_Sym) = 24

typedef struct elf32_sym{
  Elf32_Word    st_name;
  Elf32_Addr    st_value;
  Elf32_Word    st_size;
  unsigned char st_info;
  unsigned char st_other;
  Elf32_Half    st_shndx;
} Elf32_Sym;

// ---------------------------------------------------------------------------------------------
// ELF symbol table bindings & types
#define ELF_STB_LOCAL  0  // Local symbols are not visible outside the object file
#define ELF_STB_GLOBAL 1  // Global symbols are visible to all object files being combined
#define ELF_STB_WEAK   2  // Like global but with lower precedence
#define ELF_STB_LOOS   10 // reserved for operating system-specific semantics
#define ELF_STB_HIOS   12 // reserved for operating system-specific semantics
#define ELF_STB_LOPROC 13 // reserved for processor-specific semantics
#define ELF_STB_HIPROC 15 // reserved for processor-specific semantics

#define ELF_STT_NOTYPE  0  // unspecified type
#define ELF_STT_OBJECT  1  // associated with a data object, such as a variable, an array, etc.
#define ELF_STT_FUNC    2  // associated with a function or other executable code
#define ELF_STT_SECTION 3  // associated with a section
#define ELF_STT_FILE    4  // associated with the object file name
#define ELF_STT_COMMON  5  // labels an uninitialized common block
#define ELF_STT_LOOS    10 // reserved for operating system-specific semantics
#define ELF_STT_HIOS    12 // reserved for operating system-specific semantics
#define ELF_STT_LOPROC  13 // reserved for processor-specific semantics
#define ELF_STT_HIPROC  15 // reserved for processor-specific semantics

#define ELF_ST_BIND(x)   ((x) >> 4)                 /* read binding */
#define ELF_ST_TYPE(x)   (((unsigned int) x) & 0xf) /* read type */
#define ELF_ST_INFO(b,t) (((b) << 4) + ((t) & 0xf)) /* create info from binding & type */

// ---------------------------------------------------------------------------------------------
// ELF file types
#define ELF_FT_NONE   0       // No file type
#define ELF_FT_REL    1       // Relocatable file
#define ELF_FT_EXEC   2       // Executable file
#define ELF_FT_DYN    3       // Shared object file
#define ELF_FT_CORE   4       // Core file
#define ELF_FT_LOPROC 0xff00  // Processor-specific
#define ELF_FT_HIPROC 0xffff  // Processor-specific
//
// ---------------------------------------------------------------------------------------------
// ELF machine codes
typedef enum ELFMachine {
  ELF_M_NONE    = 0,
  ELF_M_386     = 3,   // Intel x86
  ELF_M_X86_64  = 62,  // AMD x86-64
  ELF_M_ARM     = 40,  // ARM 32
  ELF_M_AARCH64 = 183, // ARM 64
  ELF_M_RISCV   = 243, // RISC-V
} ELFMachine;

// ---------------------------------------------------------------------------------------------
// ELF special section indexes
//
#define ELF_SHN_UNDEF     0
//
// ELF_SHN_LORESERVE specifies the lower bound of the range of reserved indexes.
#define ELF_SHN_LORESERVE 0xff00
//
// ELF_SHN_LOPROC through ELF_SHN_HIPROC Values in this inclusive range are reserved for
// processor-specific semantics.
#define ELF_SHN_LOPROC    0xff00
#define ELF_SHN_HIPROC    0xff1f
//
// ELF_SHN_LOOS through ELF_SHN_HIOS Values in this inclusive range are reserved for
// operating system-specific semantics.
#define ELF_SHN_LOOS      0xff20
#define ELF_SHN_HIOS      0xff3f
//
// ELF_SHN_ABS specifies absolute values for the corresponding reference.
// For example, symbols defined relative to section number ELF_SHN_ABS have absolute
// values and are not affected by relocation.
#define ELF_SHN_ABS       0xfff1
//
// ELF_SHN_COMMON Symbols defined relative to this section are common symbols, such as
// FORTRAN COMMON or unallocated C external variables.
#define ELF_SHN_COMMON    0xfff2
//
// ELF_SHN_XINDEX is an escape value. It indicates that the actual section
// header index is too large to fit in the containing field and is to be found in
// another location (specific to the structure where it appears).
#define ELF_SHN_XINDEX    0xffff
//
// ELF_SHN_HIRESERVE specifies the upper bound of the range of reserved
// indexes. The system reserves indexes between ELF_SHN_LORESERVE and ELF_SHN_HIRESERVE,
// inclusive; the values do not reference the section header table. The section
// header table does not contain entries for the reserved indexes.
#define ELF_SHN_HIRESERVE 0xffff
//
// ---------------------------------------------------------------------------------------------
// Section types
#define ELF_SHT_NULL          0  // marks the section header as inactive
#define ELF_SHT_PROGBITS      1  // information defined by the program
#define ELF_SHT_SYMTAB        2  // symbol table (usually for link editing)
#define ELF_SHT_STRTAB        3  // string table
#define ELF_SHT_RELA          4  // relocation entries with explicit addends
#define ELF_SHT_HASH          5  // symbol hash table
#define ELF_SHT_DYNAMIC       6  // information for dynamic linking
#define ELF_SHT_NOTE          7  // information that marks the file in some way
#define ELF_SHT_NOBITS        8  // like PROGBITS but occupies no space in the file
#define ELF_SHT_REL           9  // relocation entries without explicit addends
#define ELF_SHT_SHLIB         10 // reserved, unused
#define ELF_SHT_DYNSYM        11 // symbol table (minimal set of dynamic linking symbols)
#define ELF_SHT_INIT_ARRAY    14 // array of pointers to initialization functions
#define ELF_SHT_FINI_ARRAY    15 // array of pointers to termination functions
#define ELF_SHT_PREINIT_ARRAY 16 // array of pointers to priority initialization functions
#define ELF_SHT_GROUP         17 // defines a section group
#define ELF_SHT_SYMTAB_SHNDX  18 // associated with a section of type ELF_SHT_SYMTAB
#define ELF_SHT_LOOS          0x60000000 // reserved for operating system-specific semantics
#define ELF_SHT_HIOS          0x6fffffff // reserved for operating system-specific semantics
#define ELF_SHT_LOPROC        0x70000000 // reserved for processor-specific semantics
#define ELF_SHT_HIPROC        0x7fffffff // reserved for processor-specific semantics
#define ELF_SHT_LOUSER        0x80000000 // reserved for application programs
#define ELF_SHT_HIUSER        0xffffffff // reserved for application programs

// ELF defines the following special sections:
//
//  Name         Type           Attributes
//  ====         ====           ==========
//  .bss         SHT_NOBITS     SHF_ALLOC+SHF_WRITE
//  .comment     SHT_PROGBITS   none
//  .data        SHT_PROGBITS   SHF_ALLOC+SHF_WRITE
//  .data1       SHT_PROGBITS   SHF_ALLOC+SHF_WRITE
//  .debug       SHT_PROGBITS   none
//  .dynamic     SHT_DYNAMIC
//  .dynstr      SHT_STRTAB     SHF_ALLOC
//  .dynsym      SHT_DYNSYM     SHF_ALLOC
//  .fini        SHT_PROGBITS   SHF_ALLOC+SHF_EXECINSTR
//  .got         SHT_PROGBITS
//  .hash        SHT_HASH       SHF_ALLOC
//  .init        SHT_PROGBITS   SHF_ALLOC+SHF_EXECINSTR
//  .interp      SHT_PROGBITS
//  .line        SHT_PROGBITS   none
//  .note        SHT_NOTE       none
//  .plt         SHT_PROGBITS
//  .rel<name>   SHT_REL
//  .rela<name>  SHT_RELA
//  .rodata      SHT_PROGBITS   SHF_ALLOC
//  .rodata1     SHT_PROGBITS   SHF_ALLOC
//  .shstrtab    SHT_STRTAB     none
//  .strtab      SHT_STRTAB
//  .symtab      SHT_SYMTAB
//  .text        SHT_PROGBITS   SHF_ALLOC+SHF_EXECINSTR
//

// ---------------------------------------------------------------------------------------------
// sh_flags
#define ELF_SHF_WRITE            0x1
#define ELF_SHF_ALLOC            0x2
#define ELF_SHF_EXECINSTR        0x4
#define ELF_SHF_MERGE            0x10
#define ELF_SHF_STRINGS          0x20
#define ELF_SHF_INFO_LINK        0x40
#define ELF_SHF_LINK_ORDER       0x80
#define ELF_SHF_OS_NONCONFORMING 0x100
#define ELF_SHF_GROUP            0x200
#define ELF_SHF_RELA_LIVEPATCH   0x00100000
#define ELF_SHF_RO_AFTER_INIT    0x00200000
#define ELF_SHF_MASKOS           0x0ff00000
#define ELF_SHF_MASKPROC         0xf0000000


// ---------------------------------------------------------------------------------------------
// ELF header

// e_ident[] indices
#define ELF_EI_CLASS   4
#define ELF_EI_DATA    5
#define ELF_EI_VERSION 6
#define ELF_EI_OSABI   7
#define ELF_EI_PAD     8

// EFL class, byte value of e_ident[EI_CLASS=4]
#define ELF_CLASS_NONE 0
#define ELF_CLASS_32   1
#define ELF_CLASS_64   2


// ELF data encoding, byte value of e_ident[EI_DATA=5]
#define ELF_DATA_NONE 0
// Encoding 2LSB specifies 2's complement values, with the least significant byte occupying the
// lowest address. I.e. little-endian. 0x01020304 => [04 03 02 01]
#define ELF_DATA_2LSB 1
// Encoding 2MSB specifies 2's complement values, with the most significant byte occupying the
// lowest address. I.e. big-endian. 0x01020304 => [01 02 03 04]
#define ELF_DATA_2MSB 2
//
// ---------------------------------------------------------------------------------------------
// ELF_PT_* -- Program segment types
//
// ELF_PT_NULL The array element is unused; other members' values are undefined.
// This type lets the program header table have ignored entries.
#define ELF_PT_NULL    0
//
// ELF_PT_LOAD The array element specifies a loadable segment, described by p_filesz
// and p_memsz. The bytes from the file are mapped to the beginning of the memory
// segment. If the segment's memory size (p_memsz) is larger than the file size
// (p_filesz), the ``extra'' bytes are defined to hold the value 0 and to follow
// the segment's initialized area. The file size may not be larger than the memory
// size. Loadable segment entries in the program header table appear in ascending
// order, sorted on the p_vaddr member.
#define ELF_PT_LOAD    1
//
// ELF_PT_DYNAMIC The array element specifies dynamic linking information
#define ELF_PT_DYNAMIC 2
//
// ELF_PT_INTERP The array element specifies the location and size of a null-terminated
// path name to invoke as an interpreter. This segment type is meaningful only for
// executable files (though it may occur for shared objects); it may not occur more
// than once in a file. If it is present, it must precede any loadable segment entry.
#define ELF_PT_INTERP  3
//
// ELF_PT_NOTE The array element specifies the location and size of auxiliary information.
#define ELF_PT_NOTE    4
//
// ELF_PT_SHLIB This segment type is reserved but has unspecified semantics.
// Programs that contain an array element of this type do not conform to the ABI.
#define ELF_PT_SHLIB   5
//
// ELF_PT_PHDR The array element, if present, specifies the location and size of the
// program header table itself, both in the file and in the memory image of the
// program. This segment type may not occur more than once in a file. Moreover, it
// may occur only if the program header table is part of the memory image of the
// program. If it is present, it must precede any loadable segment entry.
#define ELF_PT_PHDR    6
//
// ELF_PT_TLS Thread-local storage segment
#define ELF_PT_TLS     7
//
// ELF_PT_LOOS through ELF_PT_HIOS Values in this inclusive range are reserved for
// operating system-specific semantics.
#define ELF_PT_LOOS    0x60000000
#define ELF_PT_HIOS    0x6fffffff
//
// ELF_PT_LOPROC through ELF_PT_HIPROC Values in this inclusive range are reserved for
// processor-specific semantics. If meanings are specified, the processor
// supplement explains them.
#define ELF_PT_LOPROC  0x70000000
#define ELF_PT_HIPROC  0x7fffffff

// GNU/Linux specific
#define ELF_PT_GNU_EH_FRAME 0x6474e550
#define ELF_PT_GNU_PROPERTY 0x6474e553
#define ELF_PT_GNU_STACK    (ELF_PT_LOOS + 0x474e551)

//
// ---------------------------------------------------------------------------------------------
// ELF_PF_* define the permissions on sections in the program header (p_flags)
#define ELF_PF_X        0x1        // Execute
#define ELF_PF_W        0x2        // Write
#define ELF_PF_R        0x4        // Read
#define ELF_PF_MASKOS   0x0ff00000 // Reserved for operating system-specific semantics.
#define ELF_PF_MASKPROC 0xf0000000 // Reserved for processor-specific semantics.
// ---------------------------------------------------------------------------------------------
