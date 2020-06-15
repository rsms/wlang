#
# Minimal Linux program
#
# Build & Run:
#   docker run --rm -it -v "$PWD:/src" rsms/emsdk \
#     bash -c "clang -nostdlib -O0 -o mini2 mini2.s && ./mini2" ; echo $?
#
# Build in 32-bit mode: (interesting for seeing differences to 64-bit)
#   docker run --rm -it -v "$PWD:/src" rsms/emsdk \
#     bash -c "clang -nostdlib -O0 -m32 -o mini2_32 mini2.s && ./mini2_32" ; echo $?
#
# Build with debugging info:
#   docker run --rm -it -v "$PWD:/src" rsms/emsdk \
#     clang -nostdlib -O0 -g -o mini2.g mini2.s
#
# Disassemble to see LLVMs view on the binary:
#   docker run --rm -it -v "$PWD:/src" rsms/emsdk \
#     llvm-objdump -D --syms --full-contents --all-headers mini2 > mini2.dis.txt
#
# Dump to view exact contents:
#   hexdump -v -C mini2 > mini2.hex
#
.text
.globl  _start
_start:
  movl  $42, %ebx    # exit status
  movl  $1,  %eax    # syscall message "exit"
  int   $0x80        # interrupt "syscall"
