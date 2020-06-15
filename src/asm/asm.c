#include "../defs.h"
#include "../os.h"
#include "asm.h"

#include "elf/elf.h"
#include "elf/builder.h"

#include <stdio.h>


void AsmELF() {
  ELFBuilder b = {0};
  ELFBuilderInit(&b, ELF_M_X86_64, NULL);

  // symbol table
  auto symtab = ELFBuilderNewSymtab(&b, b.strtab, ".symtab");

  // text (code) segment
  auto textdata = ELFBuilderNewData(&b);
  const u8 text1[] = {
    0xbb, 0x2a, 0x00, 0x00, 0x00,  // movl  $42, %ebx
    0xb8, 0x01, 0x00, 0x00, 0x00,  // movl  $1, %eax
    0xcd, 0x80,                    // int $128
  };
  BufAppend(&textdata->buf, text1, sizeof(text1));

  // .text section
  auto textsec = ELFBuilderNewSec(&b, ".text", ELF_SHT_PROGBITS, textdata);
  textsec->flags = ELF_SHF_ALLOC | ELF_SHF_EXECINSTR;

  // program header for executable code
  auto progexe = ELFBuilderNewProg(&b, ELF_PT_LOAD, ELF_PF_R | ELF_PF_X, textdata);
  progexe->align64 = 0x200000; // 2^21

  #define ADD_SYM64(name, binding, type, value) \
    ELFSymtabAdd64(symtab, textsec->index, name, binding, type, value)

  ADD_SYM64("", ELF_STB_LOCAL, ELF_STT_SECTION, 0x0000000000400078); // section symbol
  ADD_SYM64("_start", ELF_STB_GLOBAL, ELF_STT_NOTYPE, 0x0000000000400078);
  ADD_SYM64("__bss_start", ELF_STB_GLOBAL, ELF_STT_NOTYPE, 0x0000000000600084);
  ADD_SYM64("_edata", ELF_STB_GLOBAL, ELF_STT_NOTYPE, 0x0000000000600084);
  ADD_SYM64("_end", ELF_STB_GLOBAL, ELF_STT_NOTYPE, 0x0000000000600088);

  // assemble ELF file
  Buf buf;
  BufInit(&buf, b.mem, 0);
  if (ELFBuilderAssemble(&b, &buf) != ELF_OK) {
    logerr("ELFBuilderAssemble failed");
  }

  // write to file
  if (!os_writefile("./thingy", buf.ptr, buf.len)) {
    perror("os_writefile");
  }

  // $ hexdump -v -C mini2.linux
  //
  //            0  1  2  3  4  5  6  7   8  9 10 11 12 13 14 15
  //
  // Elf64_Ehdr -- ELF header
  //
  // 00000000  7f 45 4c 46 02 01 01 00  00 00 00 00 00 00 00 00  |.ELF............|
  //           ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //                                ident
  //
  // 00000010  02 00 3e 00 01 00 00 00  78 00 40 00 00 00 00 00  |..>.....x.@.....|
  //           ~~~~~ ~~~~~ ~~~~~~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~
  //           etype   |     version          entry_addr
  //                machine
  //
  // 00000020  40 00 00 00 00 00 00 00  58 01 00 00 00 00 00 00  |@.......X.......|
  //           ~~~~~~~~~~~~~~~~~~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~
  //                   phoff                     shoff
  //
  // 00000030  00 00 00 00 40 00 38 00  01 00 40 00 05 00 04 00  |....@.8...@.....|
  //           ~~~~~~~~~~~ ~~~~~ ~~~~~  ~~~~~ ~~~~~ ~~~~~ ~~~~~
  //              flags    ehsize  |    phnum   |   shnum   |
  //                           phentsize    shentsize      shstrndx
  //
  //
  // Elf64_Phdr -- program header
  //
  // 00000040  01 00 00 00 05 00 00 00  00 00 00 00 00 00 00 00  |................|
  //           ~~~~~~~~~~~ ~~~~~~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~
  //            type=LOAD   flags=r-x           offset=0
  //
  // 00000050  00 00 40 00 00 00 00 00  00 00 40 00 00 00 00 00  |..@.......@.....|
  //           ~~~~~~~~~~~~~~~~~~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~
  //                vaddr=0x400000           paddr=0x400000
  //
  // 00000060  84 00 00 00 00 00 00 00  84 00 00 00 00 00 00 00  |................|
  //           ~~~~~~~~~~~~~~~~~~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~
  //                  filesz=132                memsz=132
  //
  // 00000070  00 00 20 00 00 00 00 00
  //           ~~~~~~~~~~~~~~~~~~~~~~~
  //                 align=2**21
  //
  //
  //                                  0x78 = section data for PROGBITS ".text" (12 bytes long)
  //                                    |
  // 00000070                           bb 2a 00 00 00 b8 01 00
  // 00000080  00 00 cd 80
  //
  //
  //                       padding
  // 00000080              00 00 00 00
  //
  //                                  0x88 = section data for SYMTAB ".symtab" (144 bytes long)
  // .symtab                            |
  // 00000080                           00 00 00 00 00 00 00 00
  // 00000090  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
  // 000000a0  00 00 00 00 03 00 01 00  78 00 40 00 00 00 00 00
  // 000000b0  00 00 00 00 00 00 00 00  06 00 00 00 10 00 01 00
  // 000000c0  78 00 40 00 00 00 00 00  00 00 00 00 00 00 00 00
  // 000000d0  01 00 00 00 10 00 01 00  84 00 60 00 00 00 00 00
  // 000000e0  00 00 00 00 00 00 00 00  0d 00 00 00 10 00 01 00
  // 000000f0  84 00 60 00 00 00 00 00  00 00 00 00 00 00 00 00
  // 00000100  14 00 00 00 10 00 01 00  88 00 60 00 00 00 00 00
  // 00000110  00 00 00 00 00 00 00 00
  //
  //                                  0x118 = section data for STRTAB ".strtab" (25 bytes long)
  //                                    |
  // 00000110                           00 5f 5f 62 73 73 5f 73  |.........__bss_s|
  // 00000120  74 61 72 74 00 5f 65 64  61 74 61 00 5f 65 6e 64  |tart._edata._end|
  // 00000130  00                                                |.
  //
  //            0x131 = section data for STRTAB ".shstrtab" (33 bytes long)
  //              |
  // 00000130     00 2e 73 79 6d 74 61  62 00 2e 73 74 72 74 61  |...symtab..strta|
  // 00000140  62 00 2e 73 68 73 74 72  74 61 62 00 2e 74 65 78  |b..shstrtab..tex|
  // 00000150  74 00                                             |t.
  //
  //                 padding
  // 00000150        00 00 00 00 00 00
  //
  //
  // Section tables (e_shnum=5) starting with an empty one. sizeof(Shdr)=64
  //
  //                                0x158 = section tables start (e_shoff)
  //                                    |
  // 00000150                           00 00 00 00 00 00 00 00
  // 00000160  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
  // 00000170  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
  // 00000180  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
  // 00000190  00 00 00 00 00 00 00 00
  //
  // 00000190                           1b 00 00 00 01 00 00 00  sh_name 1b, sh_type PROGBITS
  // 000001a0  06 00 00 00 00 00 00 00  78 00 40 00 00 00 00 00  sh_flags, sh_addralign
  // 000001b0  78 00 00 00 00 00 00 00  0c 00 00 00 00 00 00 00  sh_offset=0x78, sh_size=12
  // 000001c0  00 00 00 00 00 00 00 00  04 00 00 00 00 00 00 00  sh_link, sh_info, sh_addralign
  // 000001d0  00 00 00 00 00 00 00 00                           sh_entsize
  //
  // 000001d0                           01 00 00 00 02 00 00 00  sh_name 01, sh_type SYMTAB
  // 000001e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  sh_flags, sh_addr
  // 000001f0  88 00 00 00 00 00 00 00  90 00 00 00 00 00 00 00  sh_offset=0x88, sh_size=144
  // 00000200  03 00 00 00 02 00 00 00  08 00 00 00 00 00 00 00  sh_link, sh_info, sh_addralign
  // 00000210  18 00 00 00 00 00 00 00                           sh_entsize (18 symbols)
  //
  // 00000210                           09 00 00 00 03 00 00 00  sh_name 09, sh_type STRTAB
  // 00000220  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  sh_flags, sh_addr
  // 00000230  18 01 00 00 00 00 00 00  19 00 00 00 00 00 00 00  sh_offset=0x118, sh_size=25
  // 00000240  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00  sh_link, sh_info, sh_addralign
  // 00000250  00 00 00 00 00 00 00 00                           sh_entsize
  //
  // 00000250                           11 00 00 00 03 00 00 00  sh_name 11, sh_type STRTAB
  // 00000260  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  sh_flags, sh_addr
  // 00000270  31 01 00 00 00 00 00 00  21 00 00 00 00 00 00 00  sh_offset=0x131, sh_size=33
  // 00000280  00 00 00 00 00 00 00 00  01 00 00 00 00 00 00 00  sh_link, sh_info, sh_addralign
  // 00000290  00 00 00 00 00 00 00 00                           sh_entsize
  //

  // u8* buf = (u8*)memalloc(NULL, 4096);
  // u32 len = 0;

  // memcpy(buf + len, &eheader, sizeof(eheader)); len += sizeof(eheader);
  // memcpy(buf + len, &pheader, sizeof(pheader)); len += sizeof(pheader);
  // memcpy(buf + len, programCode, sizeof(programCode)); len += sizeof(programCode);
  // len = align2(len, 8);

  // // symtab
  // memcpy(buf + len, symbols, sizeof(symbols)); len += sizeof(symbols);
  // len = align2(len, 8);

  // memcpy(buf + len, b.strtab.ptr, b.strtab.len); len += b.strtab.len;
  // memcpy(buf + len, b.shstrtab.ptr, b.shstrtab.len); len += b.shstrtab.len;

  // len = align2(len, 8);
  // memcpy(buf + len, &sections, sizeof(sections)); len += sizeof(sections);

  // auto file = "./thingy";
  // if (!os_writefile(file, buf, len)) {
  //   perror("os_writefile");
  // }

  ELFBuilderFree(&b);
}
