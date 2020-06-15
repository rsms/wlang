#pragma once

// An ELF file has two views into its data:
// 1. the program header shows the segments used at run time, whereas
// 2. the section header lists the set of sections of the binary.

typedef struct ELFFile {
  Buf buf;
  // u64 shstrtab; // offset of Section Header string table section
  // u64 strtab;   // offset of General string table section
  // u64 symtab;   // offset of Symbol table section
  // u64 textsec;  // offset of TEXT section. NULL until created by ELFBuilderNewSection
} ELFFile;

static void ELFFileInit(ELFFile* f, Memory mem) {
  memset(f, 0, sizeof(ELFFile));
  BufInit(&f->buf, mem, 4096);
}

static void ELFFileFree(ELFFile* f) {
  BufFree(&f->buf);
}

// Access ELF-64 header
inline static Elf64_Ehdr* ELFFileEH64(ELFFile* f) {
  return (Elf64_Ehdr*)f->buf.ptr;
}

// Access ELF-64 program header
inline static Elf64_Phdr* ELFFilePH64(ELFFile* f, u32 index) {
  // Program headers follows immediately after ELF header
  return (Elf64_Phdr*)&f->buf.ptr[sizeof(Elf64_Ehdr) + (sizeof(Elf64_Phdr) * index)];
}

// Reset ELFFile for reuse
static void ELFFileReset64(ELFFile* f, u32 phcount) {
  f->buf.len = 0;
  BufAlloc(&f->buf, sizeof(Elf64_Ehdr) + (sizeof(Elf64_Phdr) * phcount));
}
static void ELFFileReset32(ELFFile* f, u32 phcount) {
  f->buf.len = 0;
  BufAlloc(&f->buf, sizeof(Elf32_Ehdr) + (sizeof(Elf32_Phdr) * phcount));
}


// inline static Elf64_Ehdr* ELFFileSHStrtab(ELFFile* f) {
//   return (Elf64_Ehdr*)f->buf.ptr;
// }
