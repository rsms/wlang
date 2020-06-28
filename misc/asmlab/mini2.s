#
# Minimal Linux program, using legacy interrupt
#
# For info on Linux syscalls, see:
#   https://github.com/torvalds/linux/blob/v3.13/arch/x86/syscalls/syscall_64.tbl#L69
#   https://github.com/torvalds/linux/blob/v3.13/arch/x86/kernel/entry_64.S#L569-L591
#
# Build & Run:
#   clang -nostdlib -O0 -o mini2.elf mini2.s && ./mini2.elf ; echo $?
#
# Disassemble:
#   llvm-objdump -D --syms --full-contents --all-headers mini2.elf > mini2.elf.dis.txt
#
# Dump to view exact contents:
#   hexdump -v -C mini2.elf > mini2.hex
#
.text
.globl  _start

_start:
  movl  $1,  %eax    # syscall message "exit"
  movl  $42, %ebx    # exit status
  int   $0x80        # interrupt "syscall"
