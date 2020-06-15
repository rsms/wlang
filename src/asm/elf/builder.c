#include "../../defs.h"

#include "elf.h"
#include "builder.h"


static ELFMode machineMode(ELFMachine m) {
  switch (m) {
    case ELF_M_386:     return ELFMode32; // Intel x86
    case ELF_M_X86_64:  return ELFMode64; // AMD x86-64
    case ELF_M_ARM:     return ELFMode32; // ARM 32
    case ELF_M_AARCH64: return ELFMode64; // ARM 64
    case ELF_M_RISCV:   return ELFMode64; // RISC-V
    case ELF_M_NONE:    break;
  }
  return ELFMode32;
}


static void addStandardSections(ELFBuilder* b) {
  assert(b->shstrtab == NULL);
  assert(b->strtab == NULL);

  // empty section to own the undefined symbol
  ELFBuilderNewSec(b, "", ELF_SHT_NULL, NULL);

  // TODO: ELF-32 / ELFMode32

  // Section header string table
  auto shstrtabdata = ELFBuilderNewData(b);
  b->shstrtab = ELFBuilderNewSec(b, "", ELF_SHT_STRTAB, shstrtabdata);
  // b->shstrtab->sh64.sh_offset    = 0x0000000000000131;
  // b->shstrtab->sh64.sh_size      = 0x0000000000000021;
  // b->shstrtab->sh64.sh_link      = 0x00000000;
  // b->shstrtab->sh64.sh_info      = 0x00000000;

  // add the name of the shstrtab itself to itself (lol)
  b->shstrtab->name = ELFStrtabAppend(b->shstrtab, ".shstrtab");

  // Generic string table
  auto strtabdata = ELFBuilderNewData(b);
  b->strtab = ELFBuilderNewSec(b, ".strtab", ELF_SHT_STRTAB, strtabdata);
  // b->strtab->sh64.sh_offset    = 0x0000000000000118;
  // b->strtab->sh64.sh_size      = 0x0000000000000019;
  // b->strtab->sh64.sh_link      = 0x00000000;
  // b->strtab->sh64.sh_info      = 0x00000000;
}


void ELFBuilderInit(ELFBuilder* b, ELFMachine m, Memory nullable mem) {
  b->mem = mem;
  b->mode = machineMode(m);
  b->machine = m;
  b->shstrtab = NULL;
  b->strtab = NULL;
  ArrayInit(&b->dv);
  ArrayInit(&b->shv);
  ArrayInit(&b->phv);
  addStandardSections(b);
}

static void ELFDataFree(ELFData* d) {
  auto mem = d->buf.mem;
  ArrayFree(&d->secv, mem);
  ArrayFree(&d->progv, mem);
  BufFree(&d->buf);
}
inline static void ELFSecFree(ELFSec* sec) {}
inline static void ELFProgFree(ELFProg* p) {}


void ELFBuilderFree(ELFBuilder* b) {
  ArrayForEach(&b->shv, ELFSec, sec) {
    ELFSecFree(sec);
    memfree(b->mem, sec);
  }
  ArrayForEach(&b->phv, ELFProg, p) {
    ELFProgFree(p);
    memfree(b->mem, p);
  }
  ArrayForEach(&b->dv, ELFData, d) {
    ELFDataFree(d);
    memfree(b->mem, d);
  }
  ArrayFree(&b->shv, b->mem);
  ArrayFree(&b->phv, b->mem);
  ArrayFree(&b->dv, b->mem);
  #if DEBUG
  memset(b, 0, sizeof(ELFBuilder));
  #endif
}


ELFData* ELFBuilderNewData(ELFBuilder* b) {
  auto data = memalloct(b->mem, ELFData);
  ArrayInitWithStorage(&data->secv, data->_secv_storage, countof(data->_secv_storage));
  ArrayInitWithStorage(&data->progv, data->_progv_storage, countof(data->_progv_storage));
  ArrayPush(&b->dv, data, b->mem);
  return data;
}


typedef enum ELFSecDataFlag {
  ELFSecDataNone,     // section does not have data
  ELFSecDataRequired, // section must have data
  ELFSecDataOptional, // section may have data
} ELFSecDataFlag;

static ELFSecDataFlag ELFSecData(u32 type) {
  switch (type) {
    // must NOT have data
    case ELF_SHT_NULL:
    case ELF_SHT_NOBITS:
    case ELF_SHT_GROUP:
      return ELFSecDataNone;

    // must have data
    case ELF_SHT_PROGBITS:
    case ELF_SHT_SYMTAB:
    case ELF_SHT_STRTAB:
    case ELF_SHT_RELA:
    case ELF_SHT_HASH:
    case ELF_SHT_REL:
    case ELF_SHT_DYNSYM:
    case ELF_SHT_INIT_ARRAY:
    case ELF_SHT_FINI_ARRAY:
    case ELF_SHT_PREINIT_ARRAY:
    case ELF_SHT_SYMTAB_SHNDX:
      return ELFSecDataRequired;

    default:
      // other sections are non-standard. Either OS, processor or user.
      return ELFSecDataOptional;
  }
}


ELFSec* ELFBuilderNewSec(ELFBuilder* b, const char* name, u32 type, ELFData* data) {
  auto df = ELFSecData(type);
  if (df == ELFSecDataRequired && data == NULL) {
    assertf(0, "data is required for section type %u", type);
    return NULL;
  } else if (df == ELFSecDataNone && data != NULL) {
    assertf(0, "data provided for section type %u which does not have data", type);
    return NULL;
  }

  auto sec = memalloct(b->mem, ELFSec);
  sec->builder = b;
  sec->data = data;
  sec->index = b->shv.len;
  sec->type = type;
  sec->name = 0;
  if (b->shstrtab != NULL) {
    sec->name = ELFStrtabAppend(b->shstrtab, name);
  }

  // section type-specific initialization
  if (type == ELF_SHT_STRTAB) {
    // string tables starts with a zero byte
    BufAppend(&data->buf, "\0", 1);
  }

  if (data != NULL) {
    ArrayPush(&data->secv, sec, b->mem);
  }
  ArrayPush(&b->shv, sec, b->mem);
  return sec;
}


ELFProg* ELFBuilderNewProg(ELFBuilder* b, u32 type, u32 flags, ELFData* data) {
  auto p = memalloct(b->mem, ELFProg);
  p->builder = b;
  p->data = data;
  p->type = type;
  p->flags = flags;
  if (data != NULL) {
    ArrayPush(&data->progv, p, b->mem);
  }
  ArrayPush(&b->phv, p, b->mem);
  return p;
}


ELFSec* ELFBuilderNewSymtab(ELFBuilder* b, const ELFSec* strtab, const char* name) {
  auto data = ELFBuilderNewData(b);
  auto sec = ELFBuilderNewSec(b, name, ELF_SHT_SYMTAB, data);
  sec->link = checknull(strtab)->index;
  if (strcmp(name, ".symtab") == 0) {
    // the standard special ".symtab" symbol table section
    // symbol #0 is both the first entry and serves as the undefined symbol (index 0)
    if (b->mode == ELFMode32) {
      ELFSymtabAdd32(sec, /*section*/0, /*name*/"", ELF_STB_LOCAL, ELF_STT_NOTYPE, 0);
    } else {
      ELFSymtabAdd64(sec, /*section*/0, /*name*/"", ELF_STB_LOCAL, ELF_STT_NOTYPE, 0);
    }
    assert(b->symtab == NULL); // duplicate
    b->symtab = sec;
  }
  return sec;
}


const char* ELFSecName(const ELFSec* sec) {
  auto b = checknull(sec->builder);
  if (b->shstrtab == NULL) {
    return "";
  }
  assert(sec->name < b->shstrtab->data->buf.len);
  return (const char*)&b->shstrtab->data->buf.ptr[sec->name];
}


u32 ELFStrtabAppend(ELFSec* sec, const char* name) {
  assert(sec->type == ELF_SHT_STRTAB); // section must be a string table
  size_t z = strlen(name);
  if (z == 0) {
    return 0; // return the null string index
  }
  if (sec->data->buf.len + (u64)z >= 0xFFFFFFFF) {
    return 0; // table is full; return the null string index
  }
  u32 offs = (u32)sec->data->buf.len;
  BufAppend(&sec->data->buf, name, z + 1); // include terminating 0
  return offs;
}


const char* ELFStrtabLookup(const ELFSec* sec, u32 nameindex) {
  assert(sec->type == ELF_SHT_STRTAB);
  assert(nameindex < sec->data->buf.len);
  return (const char*)&sec->data->buf.ptr[nameindex];
}


// Add symbol with name to symtab, originating in section with index shndx
Elf32_Sym* ELFSymtabAdd32(ELFSec* symtab, u16 shndx, const char* name, u8 bind, u8 typ, u32 value){
  assert(symtab->type == ELF_SHT_SYMTAB);
  auto b = checknull(symtab->builder);
  assert(b->strtab != NULL);

  auto sym = (Elf32_Sym*)BufAlloc(&symtab->data->buf, sizeof(Elf32_Sym));
  sym->st_name = ELFStrtabAppend(b->strtab, name);
  sym->st_value = value;
  sym->st_size = 0;
  sym->st_info = ELF_ST_INFO(bind, typ);
  sym->st_other = 0; // unused
  sym->st_shndx = shndx;
  return sym;

  // return symbol index
  // return (u32)((symtab->data->buf.len / sizeof(Elf32_Sym)) - sizeof(Elf32_Sym));
}

Elf64_Sym* ELFSymtabAdd64(ELFSec* symtab, u16 shndx, const char* name, u8 bind, u8 typ, u64 value){
  assert(symtab->type == ELF_SHT_SYMTAB);
  auto b = checknull(symtab->builder);
  assert(b->strtab != NULL);

  auto sym = (Elf64_Sym*)BufAlloc(&symtab->data->buf, sizeof(Elf64_Sym));
  sym->st_name = ELFStrtabAppend(b->strtab, name);
  sym->st_info = ELF_ST_INFO(bind, typ);
  sym->st_other = 0; // unused
  sym->st_shndx = shndx;
  sym->st_value = value;
  sym->st_size = 0;
  return sym;
}
