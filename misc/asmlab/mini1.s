#
# Minimal Linux program, using modern syscall instruction
#
# For info on Linux syscalls, see:
#   https://github.com/torvalds/linux/blob/v3.13/arch/x86/syscalls/syscall_64.tbl#L69
#   https://github.com/torvalds/linux/blob/v3.13/arch/x86/kernel/entry_64.S#L569-L591
#
# Build & Run:
#   clang -nostdlib -O0 -o mini1.elf mini1.s && ./mini1.elf ; echo $?
#
# Disassemble:
#   llvm-objdump -D --syms --full-contents --all-headers mini1.elf > mini1.elf.dis.txt
#
# Dump to view exact contents:
#   hexdump -v -C mini1.elf > mini1.hex
#
.text
.globl  _start

_start:
  movq  $60, %rax    # syscall no "exit"
  movq  $42, %rdi    # exit status
  syscall
