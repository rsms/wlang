#include "../defs.h"
#include "elf64.h"
#include "elf/elf.h"

void ELF64Init(ELF64* e, Memory nullable mem) {
  memset(e, 0, sizeof(ELF64));
  // main buffer (ELF header + program headers + data segments)
  BufInit(&e->buf, mem, 4096);
  memset(e->buf.ptr, 0, sizeof(Elf64_Ehdr)); // reserve space for ELF header
  e->buf.len += sizeof(Elf64_Ehdr);

  // section headers buffer. Section header #0 is the "null"-section header.
  BufInit(&e->shbuf, mem, sizeof(Elf64_Shdr) * 6); // usually at least 6 sections
  memset(e->shbuf.ptr, 0, sizeof(Elf64_Shdr)); // allocate the "null"-section header.
  e->shbuf.len += sizeof(Elf64_Shdr);

  // strtab; string table. starts with \0. Usually just the zero name and "_start".
  BufInit(&e->strtab, mem, 16);
  e->strtab.ptr[0] = 0; // allocate the "null" string
  e->strtab.len++;

  // shstrtab; section header string table. starts with \0
  // Usually: "", ".text", ".rodata", ".symtab", ".strtab" and ".shstrtab".
  BufInit(&e->shstrtab, mem, 64);
  e->shstrtab.ptr[0] = 0; // allocate the "null" string
  e->shstrtab.len++;

  // symtab. usually just the "null" symbol and the "_start" symbol.
  BufInit(&e->symtab, mem, sizeof(Elf64_Sym) * 4);
  memset(e->symtab.ptr, 0, sizeof(Elf64_Sym)); // allocate the "null" symbol
  e->symtab.len += sizeof(Elf64_Sym);
}

void ELF64Free(ELF64* e) {
  // TODO
}

static u32 strtabAdd(Buf* buf, const char* pch, size_t size) {
  if (size == 0) {
    return 0; // return the null string index
  }
  if (((u64)buf->len) + ((u64)size) >= 0xFFFFFFFF) {
    return 0; // table is full; return the null string index
  }
  u32 offs = (u32)buf->len;

  // append pch of size + a sentinel null byte
  BufMakeRoomFor(buf, size + 1);
  memcpy(buf->ptr + buf->len, pch, size);
  buf->ptr[buf->len + size] = 0;
  buf->len += size + 1;

  return offs;
}

// ELF64AddStr adds a string to strtab. Returns the strtab index.
inline static u32 ELF64AddStr(ELF64* e, const char* s) {
  return strtabAdd(&e->strtab, s, strlen(s));
}

// ELF64AddSym adds a symbol to the symbol table
// - shndx  section index of owning section (ELF_SHN_UNDEF for "no section")
// - bind   a ELF_STB_* constant
// - typ    a ELF_STT_* constant
// Returned pointer is valid until the next call to ELF64AddSym.
// You usually want to set st_value (and sometimes st_size) on the returned symbol.
static Elf64_Sym* ELF64AddSym(ELF64* e, const char* name, u16 shndx, u8 bind, u8 typ) {
  auto sym = (Elf64_Sym*)BufAllocz(&e->symtab, sizeof(Elf64_Sym));
  sym->st_name = strtabAdd(&e->strtab, name, strlen(name));
  sym->st_info = ELF_ST_INFO(bind, typ);
  sym->st_shndx = shndx;
  return sym;
}

// ELF64AddPH adds a program header.
// Returns program header index which can be used with ELF64GetPH to access the struct.
static u16 ELF64AddPH(ELF64* e) {
  u16 phidx = e->phnum;
  e->phnum++;
  BufAllocz(&e->buf, sizeof(Elf64_Phdr));
  return phidx;
}

inline static Elf64_Phdr* ELF64GetPH(ELF64* e, u16 phidx) {
  assert(phidx < e->phnum);
  return (Elf64_Phdr*)&e->buf.ptr[sizeof(Elf64_Ehdr) + ((size_t)phidx * sizeof(Elf64_Phdr))];
}


// ELF64AddSection adds a new section header.
// Affects e->shbuf, e->shstrtab
static Elf64_Shdr* ELF64AddSection(ELF64* e, const char* name) {
  auto sh = (Elf64_Shdr*)BufAllocz(&e->shbuf, sizeof(Elf64_Shdr));
  sh->sh_name = strtabAdd(&e->shstrtab, name, strlen(name));
  return sh;
}

// ELF64StartSection starts a new section which is assumed to have data.
// If you want a section that doesn't point to data, use ELF64AddSection.
// returned pointer only valid until next call to ELF64StartSection.
// Affects e->shbuf, e->shstrtab, e->buf
static Elf64_Shdr* ELF64StartSection(ELF64* e, const char* name, u64 sh_addralign) {
  auto align = align2(e->buf.len, sh_addralign);
  if (align != e->buf.len) {
    // add padding to force alignment
    BufAllocz(&e->buf, align - e->buf.len);
  }
  auto sh = ELF64AddSection(e, name);
  sh->sh_offset = e->buf.len; // section's data file offset
  sh->sh_addralign = sh_addralign;
  return sh;
}


static int _symcmp64(const void* ptr1, const void* ptr2) {
  auto ainfo = ((const Elf64_Sym*)ptr1)->st_info;
  auto binfo = ((const Elf64_Sym*)ptr2)->st_info;
  if (ELF_ST_BIND(ainfo) == ELF_STB_LOCAL) {
    if (ELF_ST_BIND(binfo) != ELF_STB_LOCAL) {
      return -1; // a is local, b is global
    }
    return 0; // both are local
  } if (ELF_ST_BIND(binfo) == ELF_STB_LOCAL) {
    return 1;  // a is global, b is local
  }
  return 0;  // both are global
}

// sort symbols of SYMTYPE in buf
// void SYMTAB64_SORT(const Buf* buf)
#define SYMTAB64_SORT(buf) \
  qsort((buf)->ptr, (buf)->len / sizeof(Elf64_Sym), sizeof(Elf64_Sym), &_symcmp64)

// void SYMTAB_FOR_EACH(const Buf* buf, Elf32_Sym|Elf64_Sym, NAME)
#define SYMTAB_FOR_EACH(buf, SYMTYPE, LOCALNAME) \
  for ( \
    auto LOCALNAME        = (SYMTYPE*)(buf)->ptr, \
         LOCALNAME##__end = (SYMTYPE*)&(buf)->ptr[(buf)->len]; \
    LOCALNAME < LOCALNAME##__end; \
    LOCALNAME++ \
  ) /* <body should follow here> */


static Elf64_Ehdr* finalizeEH(ELF64* e, u8 encoding) {
  auto eh = (Elf64_Ehdr*)e->buf.ptr;
  *((u32*)&eh->e_ident[0]) = *((u32*)&"\177ELF");
  eh->e_ident[ELF_EI_CLASS]   = ELF_CLASS_64;
  eh->e_ident[ELF_EI_DATA]    = encoding;
  eh->e_ident[ELF_EI_VERSION] = ELF_V_CURRENT;
  eh->e_ident[ELF_EI_OSABI]   = ELF_OSABI_NONE;
  eh->e_type                  = ELF_FT_EXEC;
  eh->e_version               = ELF_V_CURRENT;
  eh->e_flags     = 0;                   // Processor-specific flags. Usually 0.
  eh->e_ehsize    = sizeof(Elf64_Ehdr);  // ELF header's size in bytes
  eh->e_phentsize = sizeof(Elf64_Phdr);  // size of a program header
  eh->e_shentsize = sizeof(Elf64_Shdr);  // size of a section header
  return eh;
}

static void ELF64Finalize(ELF64* e, u8 encoding, u16 e_machine, u64 entryaddr) {
  // Note: If we were to write directly to a file, we could replace the BufAppend
  // calls here with file-write calls.
  //
  // When this function is called, e->buf is expected to look like this:
  //
  //    +————————————————————————————+
  //    | ELF header                 |
  //    +————————————————————————————+
  //    | Program headers            |
  //    | ...                        |
  //    +————————————————————————————+
  //    | TEXT, DATA etc segments    |
  //    | ...                        |
  //
  // This function will append the following:
  //
  //    | .symtab segment            |
  //    +————————————————————————————+
  //    | .strtab segment            |
  //    +————————————————————————————+
  //    | .shstrtab segment          |
  //    +————————————————————————————+
  //    | section headers            |
  //    | ...                        |
  //    +————————————————————————————+
  //
  // Finally the ELF header is patched with information now known about section headers,
  // program headers, virtual memory addresses, and segments.
  //

  size_t shnum = e->shbuf.len / sizeof(Elf64_Shdr); // number of section headers
  //u32 strtab_sec_index = shnum + 1;

  // .symtab section
  auto sh = ELF64StartSection(e, ".symtab", /* sh_addralign */ 8);
  sh->sh_type = ELF_SHT_SYMTAB;
  sh->sh_size = e->symtab.len;
  sh->sh_link = shnum + 1; // section index of .strtab
  sh->sh_entsize = sizeof(Elf64_Sym);
  // Sort symbols so that local bindings appear first: (a requirement of ELF)
  SYMTAB64_SORT(&e->symtab);
  u32 nlocals = 0;
  SYMTAB_FOR_EACH(&e->symtab, Elf64_Sym, sym) {
    if (ELF_ST_BIND(sym->st_info) != ELF_STB_LOCAL) {
      break;
    }
    nlocals++;
  }
  // for symtab sections, sh_info should hold one greater than the symbol table index of
  // the last local symbol.
  sh->sh_info = nlocals;
  BufAppend(&e->buf, e->symtab.ptr, e->symtab.len);

  // .strtab section
  sh = ELF64StartSection(e, ".strtab", /* sh_addralign */ 1);
  sh->sh_type = ELF_SHT_STRTAB;
  sh->sh_size = e->strtab.len;
  BufAppend(&e->buf, e->strtab.ptr, e->strtab.len);

  // .shstrtab section
  sh = ELF64StartSection(e, ".shstrtab", /* sh_addralign */ 1);
  sh->sh_type = ELF_SHT_STRTAB;
  sh->sh_size = e->shstrtab.len;
  BufAppend(&e->buf, e->shstrtab.ptr, e->shstrtab.len);

  // write section headers, saveing section header table offset for use later in PH
  u64 e_shoff = e->buf.len;
  BufAppend(&e->buf, e->shbuf.ptr, e->shbuf.len);

  // update section header count (after adding standard sections)
  shnum = e->shbuf.len / sizeof(Elf64_Shdr);
  assert(shnum <= 0xFFFF); // must fit in u16

  // update ELF header
  auto eh = finalizeEH(e, encoding);
  eh->e_machine  = e_machine;
  eh->e_phoff    = sizeof(Elf64_Ehdr); // Program header table file offset (follows ELF header)
  eh->e_phnum    = e->phnum;   // number of program headers
  eh->e_shoff    = e_shoff;    // Section header table file offset
  eh->e_shnum    = (u16)shnum; // number of section headers
  eh->e_entry    = entryaddr;  // Entry point virtual address
  eh->e_shstrndx = shnum - 1;  // section header string stable section index (last section)
}
