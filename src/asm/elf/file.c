#include "../../defs.h"
#include "../../memory.h"
#include "../../buf.h"
#include "elf.h"
#include "file.h"
#include <math.h> // for log2

static void printerr(const ELFFile* f, FILE* nullable fp, const char* fmt, ...) {
  if (fp != NULL) {
    fprintf(fp, "%s: error: ", ELFFileName(f, "<input>"));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
  }
}

static const Elf32_Shdr* ELFFileSH32Checked(const ELFFile* f, u32 index) {
  auto offs = ELFFileEH32(f)->e_shoff + (sizeof(Elf32_Shdr) * index);
  if (offs > f->len) {
    return NULL;
  }
  return (const Elf32_Shdr*)&f->buf[offs];
}


static const Elf64_Shdr* ELFFileSH64Checked(const ELFFile* f, u32 index) {
  auto offs = ELFFileEH64(f)->e_shoff + (sizeof(Elf64_Shdr) * index);
  if (offs > f->len) {
    return NULL;
  }
  return (const Elf64_Shdr*)&f->buf[offs];
}


void ELFFileInit(ELFFile* f, const char* nullable name, const u8* data, size_t len) {
  f->name = name;
  f->buf = data;
  f->len = len;
  f->shstrtab = NULL;

  if (f->len < 4) {
    return;
  }

  auto cls = ELFFileClass(f);
  if (cls == ELF_CLASS_64) {
    if (f->len >= sizeof(Elf64_Ehdr)) {
      auto eh = ELFFileEH64(f);
      if (eh->e_shstrndx != ELF_SHN_UNDEF && eh->e_shstrndx < eh->e_shnum) {
        auto shstr_sec = ELFFileSH64Checked(f, eh->e_shstrndx);
        if (shstr_sec != NULL && shstr_sec->sh_offset + shstr_sec->sh_size <= f->len) {
          f->shstrtab = (const char*)&f->buf[shstr_sec->sh_offset];
        }
      }
    }
  } else if (cls == ELF_CLASS_32) {
    if (f->len >= sizeof(Elf32_Ehdr)) {
      auto eh = ELFFileEH32(f);
      if (eh->e_shstrndx != ELF_SHN_UNDEF && eh->e_shstrndx < eh->e_shnum) {
        auto shstr_sec = ELFFileSH32Checked(f, eh->e_shstrndx);
        if (shstr_sec != NULL && shstr_sec->sh_offset + shstr_sec->sh_size <= f->len) {
          f->shstrtab = (const char*)&f->buf[shstr_sec->sh_offset];
        }
      }
    }
  }
}


bool ELFFileValidate(const ELFFile* f, FILE* fp) {
  if (f->len < 4 || memcmp(f->buf, "\177ELF", 4) != 0) {
    printerr(f, fp, "invalid ELF header (does not start with '\\x7f' 'E' 'L' 'F')");
    return false;
  }

  auto cls = ELFFileClass(f);
  if ((cls == ELF_CLASS_32 && f->len < sizeof(Elf32_Ehdr)) ||
      (cls == ELF_CLASS_64 && f->len < sizeof(Elf64_Ehdr))
  ) {
    printerr(f, fp, "invalid ELF header (too small)");
    return false;
  }

  if (f->shstrtab == NULL) {
    if (cls == ELF_CLASS_32 && ELFFileEH32(f)->e_shstrndx != ELF_SHN_UNDEF) {
      dlog("TODO validate shstrtab");
      return false;
    } else if (cls == ELF_CLASS_64 && ELFFileEH64(f)->e_shstrndx != ELF_SHN_UNDEF) {
      // something went wrong; init didn't find shstrtab but file header says one exist
      dlog("TODO validate shstrtab");
      // if (eh->e_shstrndx >= eh->e_shnum) {
      //   printerr(f, fp, "e_shstrndx >= e_shnum (%u >= %u)", eh->e_shstrndx, eh->e_shnum);
      return false;
    }
  }

  return true;
}


/*
static const char* STUB(ELFMachine m) {
  switch (m) {
    default: return "?";
  }
}
*/

static const char* sh_flags_str(u32 flags) {
  static char buf[100]; // adjust if flags change
  char* p = buf;
  #define ADDS(pch) ({             \
    memcpy(p, pch, strlen(pch));   \
    p += strlen(pch);              \
  })
  if (flags & ELF_SHF_WRITE)            { ADDS("WRITE|"); }
  if (flags & ELF_SHF_ALLOC)            { ADDS("ALLOC|"); }
  if (flags & ELF_SHF_EXECINSTR)        { ADDS("EXECINSTR|"); }
  if (flags & ELF_SHF_MERGE)            { ADDS("MERGE|"); }
  if (flags & ELF_SHF_STRINGS)          { ADDS("STRINGS|"); }
  if (flags & ELF_SHF_INFO_LINK)        { ADDS("INFO_LINK|"); }
  if (flags & ELF_SHF_LINK_ORDER)       { ADDS("LINK_ORDER|"); }
  if (flags & ELF_SHF_OS_NONCONFORMING) { ADDS("OS_NONCONFORMING|"); }
  if (flags & ELF_SHF_GROUP)            { ADDS("GROUP|"); }
  if (flags & ELF_SHF_RO_AFTER_INIT)    { ADDS("RO_AFTER_INIT|"); }
  // Don't forget to adjust size of buf above if flags change
  #undef ADDS
  if (p > buf) {
    p--; // undo last "|"
  }
  *p = 0;
  return buf;
}

static const char* ph_flags_str(u32 flags) {
  bool r = (flags & ELF_PF_R), w = (flags & ELF_PF_W), x = (flags & ELF_PF_X);
  return (
    r ?
      w ? x ? "rwx" : "rw-" :
      x ?     "r-x" : "r--" :
    w ?
      x ?     "-wx" : "-w-" :
    x ?       "--x" : "---"
  );
}

static const char* eh_type_str(u32 type) {
  switch (type) {
    case ELF_FT_NONE: return "NONE";
    case ELF_FT_REL:  return "REL";
    case ELF_FT_EXEC: return "EXEC";
    case ELF_FT_DYN:  return "DYN";
    case ELF_FT_CORE: return "CORE";
    default:
      return (
        ELF_FT_LOPROC >= type && type <= ELF_FT_HIPROC ? "PROC?" :
        "?"
      );
  }
}

const char* sh_type_str(u32 t) {
  switch (t) {
  case ELF_SHT_NULL:          return "NULL";
  case ELF_SHT_PROGBITS:      return "PROGBITS";
  case ELF_SHT_SYMTAB:        return "SYMTAB";
  case ELF_SHT_STRTAB:        return "STRTAB";
  case ELF_SHT_RELA:          return "RELA";
  case ELF_SHT_HASH:          return "HASH";
  case ELF_SHT_DYNAMIC:       return "DYNAMIC";
  case ELF_SHT_NOTE:          return "NOTE";
  case ELF_SHT_NOBITS:        return "NOBITS";
  case ELF_SHT_REL:           return "REL";
  case ELF_SHT_SHLIB:         return "SHLIB";
  case ELF_SHT_DYNSYM:        return "DYNSYM";
  case ELF_SHT_INIT_ARRAY:    return "INIT_ARRAY";
  case ELF_SHT_FINI_ARRAY:    return "FINI_ARRAY";
  case ELF_SHT_PREINIT_ARRAY: return "PREINIT_ARRAY";
  case ELF_SHT_GROUP:         return "GROUP";
  case ELF_SHT_SYMTAB_SHNDX:  return "SYMTAB_SHNDX";
  default:
    return (
      ELF_SHT_LOOS <= t && t <= ELF_SHT_HIOS ? "OS?" :
      ELF_SHT_LOPROC <= t && t <= ELF_SHT_HIPROC ? "PROC?" :
      ELF_SHT_LOUSER <= t && t <= ELF_SHT_HIUSER ?  "USER?" :
      "?"
    );
  }
}

const char* ph_type_str(u32 t) {
  switch (t) {
  case ELF_PT_NULL:         return "NULL";
  case ELF_PT_LOAD:         return "LOAD";
  case ELF_PT_DYNAMIC:      return "DYNAMIC";
  case ELF_PT_INTERP:       return "INTERP";
  case ELF_PT_NOTE:         return "NOTE";
  case ELF_PT_SHLIB:        return "SHLIB";
  case ELF_PT_PHDR:         return "PHDR";
  case ELF_PT_TLS:          return "TLS";
  case ELF_PT_GNU_EH_FRAME: return "GNU_EH_FRAME";
  case ELF_PT_GNU_PROPERTY: return "GNU_PROPERTY";
  case ELF_PT_GNU_STACK:    return "GNU_STACK";
  default:
    return (
      ELF_PT_LOOS <= t && t <= ELF_PT_HIOS     ? "OS?" :
      ELF_PT_LOPROC <= t && t <= ELF_PT_HIPROC ? "PROC?" :
      "?"
    );
  }
}


static const char* ELFMachineStr(ELFMachine m) {
  switch (m) {
    case ELF_M_NONE:           return "NONE";
    case ELF_M_M32:            return "M32 (AT&T WE 32100)";
    case ELF_M_SPARC:          return "SPARC (SPARC)";
    case ELF_M_386:            return "386 (Intel 80386)";
    case ELF_M_68K:            return "68K (Motorola 68000)";
    case ELF_M_88K:            return "88K (Motorola 88000)";
    case ELF_M_860:            return "860 (Intel 80860)";
    case ELF_M_MIPS:           return "MIPS (MIPS I/R3000 Architecture)";
    case ELF_M_S370:           return "S370 (IBM System/370 Processor)";
    case ELF_M_MIPS_RS3_LE:    return "MIPS_RS3_LE (MIPS RS3000 Little-endian)";
    case ELF_M_PARISC:         return "PARISC (Hewlett-Packard PA-RISC)";
    case ELF_M_VPP500:         return "VPP500 (Fujitsu VPP500)";
    case ELF_M_SPARC32PLUS:    return "SPARC32PLUS (Enhanced instruction set SPARC)";
    case ELF_M_960:            return "960 (Intel 80960)";
    case ELF_M_PPC:            return "PPC (PowerPC 32-bit)";
    case ELF_M_PPC64:          return "PPC64 (PowerPC 64-bit)";
    case ELF_M_S390:           return "S390 (IBM S/390 Processor)";
    case ELF_M_SPU:            return "SPU (Cell BE SPU)";
    case ELF_M_V800:           return "V800 (NEC V800)";
    case ELF_M_FR20:           return "FR20 (Fujitsu FR20)";
    case ELF_M_RH32:           return "RH32 (TRW RH-32)";
    case ELF_M_RCE:            return "RCE (Motorola RCE)";
    case ELF_M_ARM:            return "ARM (Advanced RISC Machines ARM 32-bit)";
    case ELF_M_ALPHA:          return "ALPHA (Digital Alpha)";
    case ELF_M_SH:             return "SH (Hitachi SH / SuperH)";
    case ELF_M_SPARCV9:        return "SPARCV9 (SPARC Version 9 64-bit)";
    case ELF_M_TRICORE:        return "TRICORE (Siemens TriCore embedded processor)";
    case ELF_M_ARC:            return "ARC (Argonaut RISC Core, Argonaut Technologies Inc.)";
    case ELF_M_H8_300:         return "H8_300 (Hitachi/Renesas H8/300)";
    case ELF_M_H8_300H:        return "H8_300H (Hitachi H8/300H)";
    case ELF_M_H8S:            return "H8S (Hitachi H8S)";
    case ELF_M_H8_500:         return "H8_500 (Hitachi H8/500)";
    case ELF_M_IA_64:          return "IA_64 (HP/Intel IA-64)";
    case ELF_M_MIPS_X:         return "MIPS_X (Stanford MIPS-X)";
    case ELF_M_COLDFIRE:       return "COLDFIRE (Motorola ColdFire)";
    case ELF_M_68HC12:         return "68HC12 (Motorola M68HC12)";
    case ELF_M_MMA:            return "MMA (Fujitsu MMA Multimedia Accelerator)";
    case ELF_M_PCP:            return "PCP (Siemens PCP)";
    case ELF_M_NCPU:           return "NCPU (Sony nCPU embedded RISC processor)";
    case ELF_M_NDR1:           return "NDR1 (Denso NDR1 microprocessor)";
    case ELF_M_STARCORE:       return "STARCORE (Motorola Star*Core processor)";
    case ELF_M_ME16:           return "ME16 (Toyota ME16 processor)";
    case ELF_M_ST100:          return "ST100 (STMicroelectronics ST100 processor)";
    case ELF_M_TINYJ:          return "TINYJ (Advanced Logic Corp. TinyJ embedded processor)";
    case ELF_M_X86_64:         return "X86_64 (AMD x86-64 architecture)";
    case ELF_M_PDSP:           return "PDSP (Sony DSP Processor)";
    case ELF_M_FX66:           return "FX66 (Siemens FX66 microcontroller)";
    case ELF_M_ST9PLUS:        return "ST9PLUS (STMicroelectronics ST9+ 8/16 bit microcontroller)";
    case ELF_M_ST7:            return "ST7 (STMicroelectronics ST7 8-bit microcontroller)";
    case ELF_M_68HC16:         return "68HC16 (Motorola MC68HC16 Microcontroller)";
    case ELF_M_68HC11:         return "68HC11 (Motorola MC68HC11 Microcontroller)";
    case ELF_M_68HC08:         return "68HC08 (Motorola MC68HC08 Microcontroller)";
    case ELF_M_68HC05:         return "68HC05 (Motorola MC68HC05 Microcontroller)";
    case ELF_M_SVX:            return "SVX (Silicon Graphics SVx)";
    case ELF_M_ST19:           return "ST19 (STMicroelectronics ST19 8-bit microcontroller)";
    case ELF_M_VAX:            return "VAX (Digital VAX)";
    case ELF_M_CRIS:           return "CRIS (Axis Communications 32-bit embedded processor)";
    case ELF_M_JAVELIN:        return "JAVELIN (Infineon Technologies 32-bit embedded processor)";
    case ELF_M_FIREPATH:       return "FIREPATH (Element 14 64-bit DSP Processor)";
    case ELF_M_ZSP:            return "ZSP (LSI Logic 16-bit DSP Processor)";
    case ELF_M_MMIX:           return "MMIX (Donald Knuth's educational 64-bit processor)";
    case ELF_M_HUANY:          return "HUANY (Harvard University machine-independent)";
    case ELF_M_PRISM:          return "PRISM (SiTera Prism)";
    case ELF_M_AVR:            return "AVR (Atmel AVR 8-bit microcontroller)";
    case ELF_M_FR30:           return "FR30 (Fujitsu FR30)";
    case ELF_M_D10V:           return "D10V (Mitsubishi D10V)";
    case ELF_M_D30V:           return "D30V (Mitsubishi D30V)";
    case ELF_M_V850:           return "V850 (NEC v850)";
    case ELF_M_M32R:           return "M32R (Mitsubishi/Renesas M32R)";
    case ELF_M_MN10300:        return "MN10300 (Matsushita MN10300)";
    case ELF_M_MN10200:        return "MN10200 (Matsushita MN10200)";
    case ELF_M_PJ:             return "PJ (picoJava)";
    case ELF_M_OPENRISC:       return "OPENRISC (OpenRISC 32-bit embedded processor)";
    case ELF_M_ARCOMPACT:      return "ARCOMPACT (ARCompact processor)";
    case ELF_M_XTENSA:         return "XTENSA (Tensilica Xtensa Architecture)";
    case ELF_M_BLACKFIN:       return "BLACKFIN (ADI Blackfin Processor)";
    case ELF_M_UNICORE:        return "UNICORE (UniCore-32)";
    case ELF_M_ALTERA_NIOS2:   return "ALTERA_NIOS2 (Altera Nios II soft-core processor)";
    case ELF_M_TI_C6000:       return "TI_C6000 (TI C6X DSPs)";
    case ELF_M_HEXAGON:        return "HEXAGON (QUALCOMM Hexagon)";
    case ELF_M_NDS32:          return "NDS32 (Andes Technology compact code size embedded RISC)";
    case ELF_M_AARCH64:        return "AARCH64 (Advanced RISC Machines ARM 64-bit)";
    case ELF_M_TILEPRO:        return "TILEPRO (Tilera TILEPro)";
    case ELF_M_MICROBLAZE:     return "MICROBLAZE (Xilinx MicroBlaze)";
    case ELF_M_TILEGX:         return "TILEGX (Tilera TILE-Gx)";
    case ELF_M_ARCV2:          return "ARCV2 (ARCv2 Cores)";
    case ELF_M_RISCV:          return "RISCV (RISC-V)";
    case ELF_M_BPF:            return "BPF (Linux BPF - in-kernel virtual machine)";
    case ELF_M_CSKY:           return "CSKY (C-SKY)";
    case ELF_M_FRV:            return "FRV (Fujitsu FR-V)";
    case ELF_M_CYGNUS_M32R:    return "CYGNUS_M32R (old m32r)";
    case ELF_M_S390_OLD:       return "S390_OLD (old S/390)";
    case ELF_M_CYGNUS_MN10300: return "CYGNUS_MN10300 (Panasonic/MEI MN10300, AM33)";
    default:                   return "?";
  }
}


#define _STB_NAME_MAX 6

static const char* st_info_binding_str(u8 st_info) {
  auto b = ELF_ST_BIND(st_info);
  switch (b) {
  case ELF_STB_LOCAL:  return "LOCAL";
  case ELF_STB_GLOBAL: return "GLOBAL";
  case ELF_STB_WEAK:   return "WEAK";
  default:
    return (
      ELF_STB_LOOS <= b && b <= ELF_STB_HIOS     ? "OS?" :
      ELF_STB_LOPROC <= b && b <= ELF_STB_HIPROC ? "PROC?" :
      "?"
    );
  }
}

#define _STT_NAME_MAX 7

static const char* st_info_type_str(u8 st_info) {
  auto t = ELF_ST_TYPE(st_info);
  switch (t) {
  case ELF_STT_NOTYPE:  return "NOTYPE";
  case ELF_STT_OBJECT:  return "OBJECT";
  case ELF_STT_FUNC:    return "FUNC";
  case ELF_STT_SECTION: return "SECTION";
  case ELF_STT_FILE:    return "FILE";
  case ELF_STT_COMMON:  return "COMMON";
  default:
    return (
      ELF_STB_LOOS <= t && t <= ELF_STB_HIOS     ? "OS?" :
      ELF_STB_LOPROC <= t && t <= ELF_STB_HIPROC ? "PROC?" :
      "?"
    );
  }
}


static const char* sectionName(const ELFFile* f, u32 sh_name) {
  if (f->shstrtab != NULL) {
    return &f->shstrtab[sh_name];
  }
  static char buf[13];
  snprintf(buf, 13, "#%u", sh_name);
  return buf;
}


static void printEH64(const Elf64_Ehdr* eh, FILE* fp) {
  auto ei_class   = eh->e_ident[ELF_EI_CLASS];
  auto ei_data    = eh->e_ident[ELF_EI_DATA];
  auto ei_version = eh->e_ident[ELF_EI_VERSION];
  auto ei_osabi   = eh->e_ident[ELF_EI_OSABI];

  fprintf(fp, "ELF%s encoding=%s version=%u osabi=%u\n",

    ei_class == ELF_CLASS_64 ? "64" :
    ei_class == ELF_CLASS_32 ? "32" :
    "?",

    ei_data == ELF_DATA_2LSB ? "2LSB" :
    ei_data == ELF_DATA_2MSB ? "2MSB" :
    "?",

    ei_version,
    ei_osabi
  );

  fprintf(fp,
    "  type         %s (%u)\n"
    "  machine      %s (#%u)\n"
    "  version      %u\n"
    "  entry        %016llx (VM address of program start)\n"
    "  phoff        %016llx (%lld bytes into file)\n"
    "  shoff        %016llx (%lld bytes into file)\n"
    "  flags        %u (processor specific)\n"
    "  ehsize       %u   \t(Elf64_Ehdr)\n"
    "  ph{num,size} %u, %u\t(Elf64_Phdr)\n"
    "  sh{num,size} %u, %u\t(Elf64_Shdr)\n"
    "  shstrndx     %u\n"
    "",
    eh_type_str(eh->e_type), eh->e_type,
    ELFMachineStr(eh->e_machine), eh->e_machine,
    eh->e_version,
    eh->e_entry,
    eh->e_phoff, eh->e_phoff,
    eh->e_shoff, eh->e_shoff,
    eh->e_flags,
    eh->e_ehsize,
    eh->e_phnum, eh->e_phentsize,
    eh->e_shnum, eh->e_shentsize,
    eh->e_shstrndx
  );
}


static void printPH64(const ELFFile* f, const Elf64_Phdr* ph, FILE* fp) {
  fprintf(fp,
    "      vaddr, paddr  %016llx, %016llx\n"
    "      filesz        %016llx (%llu bytes)\n"
    "      memsz         %016llx (%llu bytes)\n"
    "      align         %016llx (2**%u)\n"
    "",
    ph->p_vaddr,
    ph->p_paddr,
    ph->p_filesz, ph->p_filesz,
    ph->p_memsz, ph->p_memsz,
    ph->p_align, (u32)log2((double)ph->p_align)
  );
}


static void printSH64(const ELFFile* f, const Elf64_Shdr* sh, FILE* fp) {
  const char* info_extra = "";
  if (sh->sh_type == ELF_SHT_SYMTAB) {
    info_extra = " (locals count)";
  }

  fprintf(fp,
    "      info  %u%s, align %u, entsize %llu\n",
    sh->sh_info, info_extra, sh->sh_addralign, sh->sh_entsize);

  if (sh->sh_flags != 0) {
    fprintf(fp, "      flags %08llx (%s)\n", sh->sh_flags, sh_flags_str(sh->sh_flags));
  }

  if (sh->sh_link != ELF_SHN_UNDEF) {
    auto sh2 = ELFFileSH64Checked(f, sh->sh_link);
    if (sh2 != NULL) {
      fprintf(fp,
      "      link  #%u \"%s\"\n", sh->sh_link, sectionName(f, sh2->sh_name));
    } else {
      fprintf(fp,
      "      link  #%u (invalid section)\n", sh->sh_link);
    }
  }
}


static void print64(const ELFFile* f, FILE* fp) {
  auto eh = ELFFileEH64(f);
  printEH64(eh, stdout);

  int shnamelenmax = 0;
  int shtypenamelenmax = 0;
  int phtypenamelenmax = 0;
  for (u32 i = 0; i < eh->e_shnum; i++) {
    auto sh = ELFFileSH64(f, i);
    shnamelenmax = max(shnamelenmax,   strlen(sectionName(f, sh->sh_name)));
    shtypenamelenmax = max(shtypenamelenmax, strlen(sh_type_str(sh->sh_type)));
  }
  for (u32 i = 0; i < eh->e_phnum; i++) {
    auto ph = ELFFilePH64(f, i);
    phtypenamelenmax = max(phtypenamelenmax, strlen(ph_type_str(ph->p_type)));
  }

  // program headers
  if (eh->e_phnum == 0) {
    fprintf(fp, "\n  No program headers.\n");
  } else {
    fprintf(fp,
      "\n"
      "  Program headers:\n"
      "    Idx   %-*s  Flags         VM address        File offset & size\n",
      phtypenamelenmax, "Type"
    );
    for (u32 i = 0; i < eh->e_phnum; i++) {
      auto ph = ELFFilePH64(f, i);
      fprintf(fp,
        "    #%-3u  %-*s  %-3s %08x  %016llx  %08llx  %8llu\n",
        i,
        phtypenamelenmax, ph_type_str(ph->p_type),
        ph_flags_str(ph->p_flags), ph->p_flags,
        ph->p_vaddr,
        ph->p_offset, ph->p_filesz
      );
    }
    for (u32 i = 0; i < eh->e_phnum; i++) {
      auto ph = ELFFilePH64(f, i);
      fprintf(fp, "    program header #%u:\n", i);
      printPH64(f, ph, stdout);
    }
  }

  // sections
  fprintf(fp,
    "\n"
    "  Section headers:\n"
    "    Idx  %-*s  %-*s  VM address        File offset & size\n",
    shnamelenmax, "Name",
    shtypenamelenmax, "Type"
  );
  for (u32 i = 0; i < eh->e_shnum; i++) {
    auto sh = ELFFileSH64(f, i);
    fprintf(fp,
      "    #%-3u %-*s  %-*s  %016llx  %08llx  %8llu\n",
      i,
      shnamelenmax, sectionName(f, sh->sh_name),
      shtypenamelenmax, sh_type_str(sh->sh_type),
      sh->sh_addr,
      sh->sh_offset,
      sh->sh_size
    );
  }

  const Elf64_Shdr* sh_symtab = NULL;
  const Elf64_Shdr* sh_strtab = NULL;
  const char* strtab = NULL;

  for (u32 i = 0; i < eh->e_shnum; i++) {
    auto sh = ELFFileSH64(f, i);
    fprintf(fp, "    section header #%u %s\n", i, sectionName(f, sh->sh_name));
    printSH64(f, sh, stdout);
    if (sh->sh_type == ELF_SHT_SYMTAB) {
      if (strcmp(sectionName(f, sh->sh_name), ".symtab") == 0) {
        if (sh_symtab != NULL) {
          printerr(f, fp, "duplicate .symtab sections");
        }
        sh_symtab = sh;
      }
    } else if (sh->sh_type == ELF_SHT_STRTAB) {
      if (strcmp(sectionName(f, sh->sh_name), ".strtab") == 0) {
        if (sh_strtab != NULL) {
          printerr(f, fp, "duplicate .strtab sections");
        }
        sh_strtab = sh;
        strtab = (const char*)&f->buf[sh->sh_offset];
      }
    }
  }

  // symbols
  if (sh_symtab == NULL) {
    fprintf(fp, "\n  No symbols (no .symtab section).\n");
  } else {
    auto sh = sh_symtab;
    u64 nsyms = sh->sh_size / sh->sh_entsize;

    int strtablenmax = 0;
    if (strtab != NULL) {
      for (u64 i = 0; i < nsyms; i++) {
        auto sym = (const Elf64_Sym*)&f->buf[sh->sh_offset + (i * sizeof(Elf64_Sym))];
        strtablenmax = max(strtablenmax, strlen(&strtab[sym->st_name]));
      }
    }

    fprintf(fp, "\n  %llu symbols in .symtab:\n", nsyms);
    fprintf(fp, "    Idx   %-*s  %-*s    Value             %-*s      Size  Section\n",
      _STB_NAME_MAX, "Bind",
      strtablenmax, "Name",
      _STT_NAME_MAX, "Type"
    );
    for (u64 i = 0; i < nsyms; i++) {
      auto sym = (const Elf64_Sym*)&f->buf[sh->sh_offset + (i * sizeof(Elf64_Sym))];
      fprintf(fp, "    #%-3llu  %-*s  ", i, _STB_NAME_MAX, st_info_binding_str(sym->st_info));
      if (strtab == NULL) {
        fprintf(fp, "%u", sym->st_name);
      } else {
        fprintf(fp, "\"%s\"%*s",
          &strtab[sym->st_name],
          (int)(strtablenmax - strlen(&strtab[sym->st_name])), ""
        );
      }

      auto sh2 = ELFFileSH64(f, sym->st_shndx);
      if (sym->st_shndx == ELF_SHN_XINDEX) {
        // If st_shndx == SHN_XINDEX, then the actual section header index is too
        // large to fit in this field. The actual value is contained in the associated
        // section of type SHT_SYMTAB_SHNDX.
        // -- TODO --
        sh2 = NULL;
      }

      fprintf(fp, "  %016llx  %-*s  %8llu  #%u %s\n",
        sym->st_value,
        _STT_NAME_MAX, st_info_type_str(sym->st_info),
        sym->st_size,
        sym->st_shndx,
        sh2 == NULL ? "(XINDEX)" : sectionName(f, sh2->sh_name)
      );
    }
  }
}


static void print32(const ELFFile* f, FILE* fp) {
  // TODO
}


void ELFFilePrint(const ELFFile* f, FILE* fp) {
  if (ELFFileClass(f) == ELF_CLASS_64) {
    print64(f, fp);
  } else if (ELFFileClass(f) == ELF_CLASS_32) {
    print32(f, fp);
  } else {
    fprintf(fp, "  Unexpected ELF class 0x%02x\n", ELFFileClass(f));
  }
}
