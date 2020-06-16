#include "../../defs.h"
#include "elf.h"
#include "builder.h"

#include <stdlib.h> // for qsort_r

// ELF File layout:
//
//   ELF header
//
//   Program header 1
//   Program header 2
//   ...
//   Program header N
//
//   data N
//   ...
//   data 2
//   data 1
//
//   section header 1
//   section header 2
//   ...
//   section header N
//

static int _symcmp(void* userdata, const void* ptr1, const void* ptr2) {
  auto info_offs = (size_t)userdata;
  auto ainfo = ((const u8*)ptr1)[info_offs];
  auto binfo = ((const u8*)ptr2)[info_offs];
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
#define SYMTAB_SORT(buf, SYMTYPE) \
  qsort_r((buf)->ptr, \
          (buf)->len / sizeof(SYMTYPE), \
          sizeof(SYMTYPE), \
          (void*)offsetof(SYMTYPE, st_info), \
          &_symcmp)


#define SYMTAB_FOR_EACH(buf, SYMTYPE, LOCALNAME) \
  for ( \
    auto LOCALNAME        = (SYMTYPE*)(buf)->ptr, \
         LOCALNAME##__end = (SYMTYPE*)&(buf)->ptr[(buf)->len]; \
    LOCALNAME < LOCALNAME##__end; \
    LOCALNAME++ \
  ) /* <body should follow here> */


static ELFErr asm32Symtab(ELFBuilder* b, ELFSec* sec) {
  assert(sec->data != NULL);
  Buf* buf = &sec->data->buf;
  SYMTAB_SORT(buf, Elf32_Sym);
  // TODO
  return ELF_OK;
}


static ELFErr asm64Symtab(ELFBuilder* b, ELFSec* sec, Elf64_Shdr* sh, const ELFSec** shvorig) {
  assert(sec->data != NULL);
  // Sort symbols so that local binds appear first; a requirement of ELF.
  Buf* buf = &sec->data->buf;
  SYMTAB_SORT(buf, Elf64_Sym);

  // find end of locals and update section index
  u32 localsCount = 0;
  SYMTAB_FOR_EACH(buf, Elf64_Sym, sym) {
    if (ELF_ST_BIND(sym->st_info) == ELF_STB_LOCAL) {
      localsCount++;
    }
    if (sym->st_shndx != ELF_SHN_UNDEF) {
      // sections may have different final index at this point.
      // The original sec.index is the offset into the original b.shv
      assert(sym->st_shndx < b->shv.len);
      sym->st_shndx = shvorig[sym->st_shndx]->index;
    }
  }

  // update header structure
  sh->sh_addralign = 8;
  sh->sh_entsize = sizeof(Elf64_Sym);
  // for symtab sections, sh_info should hold one greater than the symbol table index of
  // the last local symbol.
  sh->sh_info = localsCount;

  return ELF_OK;
}


static void sortDataSegs(ELFBuilder* b) {
  // data segment layout, assigning positions (index) to segments.
  // places data for shstrtab, strtab and symtab data at the bottom.
  auto mem = b->mem;

  // compact array
  u32 dst = 0, src = 0;
  for (; src < b->dv.len; src++) {
    auto d = (ELFData*)b->dv.v[src];
    auto sec = d->secv.v[0];
    if (sec == NULL || (sec != b->shstrtab && sec != b->strtab && sec != b->symtab)) {
      b->dv.v[dst++] = d;
    }
  }
  b->dv.len -= src - dst;

  // add special data segments in predefined order (symtab, strtab, shstrtab)
  if (b->symtab != NULL) {
    ArrayPush(&b->dv, b->symtab->data, mem);
  }
  ArrayPush(&b->dv, b->strtab->data, mem);
  ArrayPush(&b->dv, b->shstrtab->data, mem);
}


static void sortSections(ELFBuilder* b) {
  auto mem = b->mem;

  #if DEBUG
  auto orig_shvlen = b->shv.len;
  #endif

  // compact array, moving all non-special sections together at the top
  u32 dst = 0, src = 0;
  for (; src < b->shv.len; src++) {
    auto sec = (ELFSec*)b->shv.v[src];
    if (sec != b->shstrtab && sec != b->strtab && sec != b->symtab) {
      sec->index = dst;
      b->shv.v[dst++] = sec;
    }
  }
  b->shv.len -= src - dst;

  // add special sections in predefined order (symtab, strtab, shstrtab)
  if (b->symtab != NULL) {
    b->symtab->index = b->shv.len;
    ArrayPush(&b->shv, b->symtab, mem);
  }

  b->strtab->index = b->shv.len;
  ArrayPush(&b->shv, b->strtab, mem);

  b->shstrtab->index = b->shv.len;
  ArrayPush(&b->shv, b->shstrtab, mem);

  #if DEBUG
  assert(orig_shvlen == b->shv.len);
  #endif
}


static u32 secAlign(ELFBuilder* b, const ELFSec* sec) {
  // TODO: currently pretty much hard-coded to x86_64.
  switch (sec->type) {
    case ELF_SHT_PROGBITS:
      return 4;

    case ELF_SHT_SYMTAB:
      return b->mode == ELFMode32 ? 4 : 8;

    default:
      return 1;
  }
}


static ELFErr asm64(ELFBuilder* b, Buf* buf) {
  Elf64_Addr vaddrBase = 0x0000000000400000; // virtual address base (2^22)

  // Start with allocating data in the buffer for ELF header and program headers.
  // This allows us to write data directly to buf.
  u64 headersSize = sizeof(Elf64_Ehdr) + (sizeof(Elf64_Phdr) * b->phv.len);
  BufAppendFill(buf, 0, headersSize);

  // Sort data segments and sections.
  // First, make a copy of sections order so we can remap symtabs.
  auto shvorig = (const ELFSec**)memalloc(b->mem, b->shv.len * sizeof(ELFSec*));
  memcpy(shvorig, b->shv.v, b->shv.len * sizeof(ELFSec*));
  sortDataSegs(b);
  sortSections(b);

  // pre-process sections
  // - setup ElfXX_Shdr
  // - sort symbol tables
  ArrayForEach(&b->shv, ELFSec, sec) {
    auto sh = &sec->sh64;

    sh->sh_name      = sec->name;
    sh->sh_type      = sec->type;
    sh->sh_flags     = sec->flags; // ok that sh_flags is wider than flags
    sh->sh_addr      = 0; // may be set later when writing section headers
    sh->sh_offset    = 0; // may be set later when writing section headers
    sh->sh_size      = 0; // may be set later when writing section headers
    sh->sh_link      = sec->link == NULL ? ELF_SHN_UNDEF : sec->link->index;
    sh->sh_info      = 0;
    sh->sh_addralign = secAlign(b, sec);
    sh->sh_entsize   = 0; // depends on type

    if (sec->type == ELF_SHT_SYMTAB) {
      auto err = asm64Symtab(b, sec, sh, shvorig);
      if (err != 0) {
        memfree(b->mem, shvorig);
        return err;
      }
    }
  }

  // TODO: strtab compression. Two options:
  //
  // 1. just-in-time when adding a string to a strtab. For this to not be too complex,
  //    a new constraint is that strings can't be removed (since other strings may reference
  //    their substrings.)
  //
  // 2. post-process in asm functions. Requires visiting all syms and sections to rewrite
  //    string index values.
  //
  // Approach 1 is likely the best approach.
  //

  // Write data segments (also assigns offsets)
  ArrayForEach(&b->dv, ELFData, d) {
    u32 sh_addralign = 1;
    ArrayForEach(&d->secv, ELFSec, sec) {
      sh_addralign = max(sh_addralign, sec->sh64.sh_addralign);
    }

    // grow buffer if needed up front to avoid two growths with alignment
    // BufMakeRoomFor(buf, d->buf.len + sh_addralign);

    // alignment
    auto padding = align2(buf->len, sh_addralign) - buf->len;
    if (padding > 0) {
      BufAppendFill(buf, 0, padding);
    }

    d->offs64 = buf->len;

    // write
    BufAppend(buf, d->buf.ptr, d->buf.len);
  }

  // save offset to section headers (need it later in ELF header)
  u64 shoff = buf->len;


  // Write section headers
  BufMakeRoomFor(buf, sizeof(Elf64_Shdr) * b->shv.len); // allocate space up-front
  ArrayForEach(&b->shv, ELFSec, sec) {
    auto sh = &sec->sh64;
    if (sec->data != NULL) {
      if (sec->data->progv.len > 0) {
        sh->sh_addr = vaddrBase + sec->data->offs64;
      }
      sh->sh_offset = sec->data->offs64;
      sh->sh_size   = sec->data->buf.len;
    }
    BufAppend(buf, sh, sizeof(Elf64_Shdr));
  }


  // Patch program headers
  ArrayForEach(&b->phv, ELFProg, p) {
    auto ph = (Elf64_Phdr*)&buf->ptr[sizeof(Elf64_Ehdr) + (sizeof(Elf64_Phdr) * p__i)];

    ph->p_type = p->type;
    ph->p_flags = p->flags;
    ph->p_align  = p->align64; // Segment alignment, file & memory

    // TODO: add support for ph without a concrete data, like STACK.
    // probably easiest to add a member to ELFProg for setting vaddr, memsz etc.

    if (p->data != NULL) {
      ph->p_offset = p->data->offs64 - headersSize; // Segment file offset
      ph->p_vaddr  = vaddrBase + ph->p_offset; // Segment virtual address
      ph->p_paddr  = ph->p_vaddr; // Segment physical address
      ph->p_filesz = headersSize + p->data->buf.len; // Segment size in file
      ph->p_memsz  = ph->p_filesz; // Segment size in memory
    }
  }


  // Patch ELF header
  auto eh = (Elf64_Ehdr*)buf->ptr;

  *((u32*)&eh->e_ident[0]) = *((u32*)&"\177ELF");
  eh->e_ident[ELF_EI_CLASS]   = ELF_CLASS_64;
  eh->e_ident[ELF_EI_DATA]    = b->encoding;
  eh->e_ident[ELF_EI_VERSION] = ELF_V_CURRENT;
  eh->e_ident[ELF_EI_OSABI]   = ELF_OSABI_NONE;

  eh->e_type      = ELF_FT_EXEC;
  eh->e_machine   = b->machine;
  eh->e_version   = ELF_V_CURRENT;

  // e_entry -- Entry point virtual address. Gives the virtual address to which the system
  // first transfers control, thus starting the process.
  assert(b->phv.len > 0); // must have at least one program header when making a FT_EXEC
  auto p = (ELFProg*)b->phv.v[0];
  assert(p->data != NULL); // first program header must have data (text data)
  eh->e_entry = vaddrBase + p->data->offs64;

  eh->e_phoff     = sizeof(Elf64_Ehdr);  // Program header table file offset
  eh->e_shoff     = shoff;               // Section header table file offset
  eh->e_flags     = 0;                   // Processor-specific flags. Usually 0.
  eh->e_ehsize    = sizeof(Elf64_Ehdr);  // ELF header's size in bytes
  eh->e_phentsize = sizeof(Elf64_Phdr);  // size of a program header
  eh->e_phnum     = b->phv.len;          // number of program headers
  eh->e_shentsize = sizeof(Elf64_Shdr);  // size of a section header
  eh->e_shnum     = b->shv.len;          // number of section headers

  // e_shstrndx holds the section header table index of the entry associated with the section
  // name string table. If the file has no section name string table, this member holds the
  // value ELF_SHN_UNDEF. See ELF_SHN_* and "String Table" for more information.
  // If the section name string table section index is greater than or equal to
  // ELF_SHN_LORESERVE (0xff00), this member has the value ELF_SHN_XINDEX (0xffff)
  // and the actual index of the section name string table section is contained in the sh_link
  // field of the section header at index 0. (Otherwise, the sh_link member of the initial
  // entry contains 0.)
  eh->e_shstrndx = (b->shstrtab != NULL) ? b->shstrtab->index : ELF_SHN_UNDEF;

  memfree(b->mem, shvorig);

  return ELF_OK;
}


ELFErr asm32(ELFBuilder* b, Buf* buf) {
  BufAlloc(buf, sizeof(Elf32_Ehdr) + (sizeof(Elf32_Phdr) * b->phv.len));

  return ELF_OK;
}


ELFErr ELFBuilderAssemble(ELFBuilder* b, Buf* buf) {
  if (b->mode == ELFMode32) {
    return asm32(b, buf);
  }
  return asm64(b, buf);
}
