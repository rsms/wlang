/*
Minimal program with rodata

VM with llvm preinstalled:
	docker run --rm -it -v "$PWD:/src" rsms/emsdk

Build & Run
	clang -nostdlib -O0 -o hello1.elf hello1.s && ./hello1.elf ; echo $?

See exact contents:
	llvm-objdump -D --syms --full-contents --all-headers hello1.elf > hello1.elf.dis.txt
	hexdump -v -C hello1.elf > hello1.elf.hex

GP regs: eax, ebx, ecx, edx, edi, esi
Special regs:
- ebp    base pointer (end address of current stack frame)
- esp    current top of the stack (end address of stack)
- eip    instruction pointer
- eflags

Stack notes:
	movl (%esp), %eax   # indirect addressing. copy top of stack to eax
	movl 4(%esp), %eax  # base pointer addressing. copy second item on stack to eax

*/
// constants, system calls
.equ SYS_OPEN,  5
.equ SYS_WRITE, 4
.equ SYS_READ,  3
.equ SYS_CLOSE, 6
.equ SYS_EXIT,  1
//
// options for open (look at usr/include/asm/fcntl.h for various values. You can combine them
// by adding them or OR-ing them)
.equ O_RDONLY, 		    0
.equ O_CREAT_WRONLY_TRUNC, 03101
//
// standard file descriptors
.equ STDIN,  0
.equ STDOUT, 1
.equ STDERR, 2
//
// misc
.equ SYSCALL,	0x80		# Linux syscall interrupt code
.equ EOF,		0			# End of file code

.section .bss
 // Buffer - this is where the data is loaded into from the data file and written from
 // into the output file. This should never exceed 16,000 for various reasons.
.equ   BUFFER_SIZE, 500
.lcomm BUFFER_DATA, BUFFER_SIZE

// 4000ff

// // .data contains mutable constant data
// .section .data
// data_items:								# Array of naturally-wide integers
// 	.long	3,67,34,222,45,75,54,34,44,33,22,11,66,0
// 	.size	data_items, 112				# 14 * 8 (sizeof long)
// 	.type	data_items, @object			# mark as "object" in ELF symbol table

// .text contains immutable executable data
.section .text
.globl _start

write_hello_to_stdout: // ()->()
	pushq		%rbp					# save stack pointer on stack
	movq		%rsp,		%rbp		# make stack pointer the base pointer
	movq		$SYS_WRITE,	%rax		# syscall msg id "write to fd"
	movq  		$STDOUT,	%rbx		# fd = STDOUT
	movabsq  	$hellomsg,	%rcx		# buffer pointer (becomes VMA addr into .rodata)
	movq		$12,		%rdx		# buffer size
	int			$SYSCALL
	movq 		%rbp,		%rsp		# restore stack pointer
	popq 		%rbp					# restore base pointer
	ret

// main:
// 	pushq		%rbp					# save stack pointer on stack
// 	movq		%rsp,		%rbp		# make stack pointer the base pointer
// 	subq		$8,			%rsp		# reserve 2xi32 on stack
// 	call		write_hello_to_stdout
// 	call		write_hello_to_stdout
// 	call		write_hello_to_stdout
// 	movq 		%rbp,		%rsp		# restore stack pointer
// 	popq 		%rbp					# restore base pointer
// 	movl		$9,			%eax		# return value 9
// 	ret									# jump to address at 8(%rbp) & increment %rbp

main:
	pushq		%rbp					# save stack pointer on stack
	movq		%rsp,		%rbp		# make stack pointer the base pointer
	subq		$4,			%rsp		# reserve 1xi32 on stack (else call would mess up stack)
	movl		$0,			-4(%rbp)	# put 0 in stack slot 1 (local0)
  for_cond:
	cmpl		$3,			-4(%rbp)	# compare local0 with 3 (how many times we print)
	jge			for_end					# if local0 >= 3 then jump to end
	call		write_hello_to_stdout
	movl		-4(%rbp),	%eax		# load local1 into rAX
	addl		$1,			%eax		# add 1 to rAX
	movl		%eax,		-4(%rbp)	# store rAX to local1
	jmp			for_cond				# loop
  for_end:
	movl		-4(%rbp),	%eax		# return the number of times we printed "Hello"
	// movl		$9,			%eax		# return value 9
	movq 		%rbp,		%rsp		# restore stack pointer
	popq 		%rbp					# restore base pointer
	ret									# jump to address at 8(%rbp) & increment %rbp

_start:
	call		main					# call main; return value is in %eax
	// movl		$42,		%ebx		# exit status in ebx
	movl		%eax,		%ebx		# %ebx holds the return status
	movl		$SYS_EXIT,	%eax		# %eax holds the syscall message ID (1="exit")
	int			$SYSCALL


.section .rodata
hellomsg:
	.asciz	"Hello world\n"
	.size	hellomsg, 13
	.type	hellomsg, @object

.section .data
hellomsg2:
	.asciz	"O hai world\n"
	.size	hellomsg2, 13
	.type	hellomsg2, @object


// // example that exists with the CLI argument count
// _start:
// 	movq  		%rsp, 		%rbp
// 	movl		0(%rbp),	%ebx		# argc is at stack pointer
// 	# Note: 8(%rbp)=argv[0], 16(%rbp)=argv[1], ... (64-bit)
// 	movl		$SYS_EXIT,	%eax		# %eax holds the syscall message ID (1="exit")
// 	int			$IN_LINUX_SYSCALL		# interrupt "syscall"


// // minimal program that writes "hello world\n" to stdout
// _start:
// 	movq		$SYS_WRITE,	%rax		# syscall msg id "write to fd"
// 	movq  		$STDOUT,	%rbx		# fd = STDOUT
// 	movabsq  	$hellomsg,	%rcx		# buffer pointer (becomes VMA addr into .rodata)
// 	movq		$13,		%rdx		# buffer size
// 	int			$SYSCALL
// 	movl		$0,			%ebx		# %ebx holds the return status
// 	movl		$SYS_EXIT,	%eax		# %eax holds the syscall message ID (1="exit")
// 	int			$SYSCALL


// // the most minimal program; just exit (using classic interrupt syscall)
// _start:
//   movl  $42, %ebx    # exit status
//   movl  $1,  %eax    # syscall message "exit"
//   int   $0x80        # interrupt "syscall"

// // the most minimal program; just exit (using modern syscall op)
// // See: https://github.com/torvalds/linux/blob/v3.13/arch/x86/syscalls/syscall_64.tbl#L69
// // See: https://github.com/torvalds/linux/blob/v3.13/arch/x86/kernel/entry_64.S#L569-L591
// _start:
//   movq  $60, %rax    # syscall no "exit"
//   movq  $42, %rdi    # exit status
//   syscall
