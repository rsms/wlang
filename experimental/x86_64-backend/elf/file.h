#pragma once

// An ELF file has two views into its data:
// 1. the program header shows the segments used at run time, whereas
// 2. the section header lists the set of sections of the binary.

typedef struct ELFFile {
  const char* nullable name;
  const u8*            buf;
  size_t               len;
  const char*          shstrtab; // pointer into buf of shstrtab. NULL if none.
} ELFFile;

void ELFFileInit(ELFFile* f, const char* nullable name, const u8* data, size_t len);
bool ELFFileValidate(const ELFFile* f, FILE* nullable errlogfp);
static const char* ELFFileName(const ELFFile* f, const char* defaultname);

// Access basic information
static u8 ELFFileClass(const ELFFile* f); // ELF_CLASS_{NONE,32,64}

// Access headers
static const Elf32_Ehdr* ELFFileEH32(const ELFFile* f);
static const Elf64_Ehdr* ELFFileEH64(const ELFFile* f);
static const Elf32_Phdr* ELFFilePH32(const ELFFile* f, u32 index);
static const Elf64_Phdr* ELFFilePH64(const ELFFile* f, u32 index);
static const Elf32_Shdr* ELFFileSH32(const ELFFile* f, u32 index);
static const Elf64_Shdr* ELFFileSH64(const ELFFile* f, u32 index);

// Print human-readable information
void ELFFilePrint(const ELFFile* f, FILE* fp);


// ----------------------------------------------------------
// inline implementations

inline static const char* ELFFileName(const ELFFile* f, const char* defaultname) {
  return f->name == NULL ? defaultname : f->name;
}

inline static u8 ELFFileClass(const ELFFile* f) {
  return f->buf[ELF_EI_CLASS];
}

inline static const Elf32_Ehdr* ELFFileEH32(const ELFFile* f) {
  return (const Elf32_Ehdr*)f->buf;
}
inline static const Elf64_Ehdr* ELFFileEH64(const ELFFile* f) {
  return (const Elf64_Ehdr*)f->buf;
}

inline static const Elf32_Phdr* ELFFilePH32(const ELFFile* f, u32 index) {
  auto eh = ELFFileEH32(f);
  return (const Elf32_Phdr*)&f->buf[eh->e_phoff + (sizeof(Elf32_Phdr) * index)];
}
inline static const Elf64_Phdr* ELFFilePH64(const ELFFile* f, u32 index) {
  auto eh = ELFFileEH64(f);
  return (const Elf64_Phdr*)&f->buf[eh->e_phoff + (sizeof(Elf64_Phdr) * index)];
}

inline static const Elf32_Shdr* ELFFileSH32(const ELFFile* f, u32 index) {
  auto eh = ELFFileEH32(f);
  return (const Elf32_Shdr*)&f->buf[eh->e_shoff + (sizeof(Elf32_Shdr) * index)];
}
inline static const Elf64_Shdr* ELFFileSH64(const ELFFile* f, u32 index) {
  auto eh = ELFFileEH64(f);
  return (const Elf64_Shdr*)&f->buf[eh->e_shoff + (sizeof(Elf64_Shdr) * index)];
}
