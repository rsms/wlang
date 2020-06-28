#
# Minimal Linux program, using modern sysenter instruction (32-bit)
# Note that 32-bit x86 does not have the syscall instruction of AMD-origin, but instead
# a sysenter instruction.
#
# For info on Linux syscalls, see:
#   https://github.com/torvalds/linux/blob/v3.13/arch/x86/syscalls/syscall_32.tbl
#   https://github.com/torvalds/linux/blob/v3.13/arch/x86/kernel/entry_32.S
#
# Build & Run:
#   clang -nostdlib -O0 -m32 -o mini1-32.elf mini1-32.s && ./mini1-32.elf ; echo $?
#
# Disassemble:
#   llvm-objdump -D --syms --full-contents --all-headers mini1-32.elf > mini1-32.elf.dis.txt
#
# Dump to view exact contents:
#   hexdump -v -C mini1-32.elf > mini1-32.elf.hex
#
.text
.globl  _start

_start:
  movl  $1,  %eax    # sysenter number for "exit"
  movl  $42, %ebx    # exit status
  movl   %esp, %ebp
  sysenter
