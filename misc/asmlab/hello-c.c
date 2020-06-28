/*

Build:
  clang -O0 -S -o hello-c.s hello-c.c
  clang -O0 -o hello-c.elf hello-c.c

Dump ELF & disassembly:
  llvm-objdump -D --syms --full-contents --all-headers hello-c.elf > hello-c.elf.dis.txt

*/

#include <unistd.h>

int main() {
  char *str = "Hello World\n";
  for (int i = 0; i < 3; i++) {
    write(1, str, strlen(str));
  }
  return 9;
}
