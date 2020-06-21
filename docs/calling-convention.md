# Calling convention

Considerations:

- Where are arguments stored? (Stack, registers, which registers?)
- Where are return values stored?
- Are registers caller-save, callee-save or a hybrid?

## Where are arguments & return values stored?

Go stores all arguments and return values on the stack and does not use registers at all.
The Go team have discussed changing this and make use of registers.
They approximate a 5-10%[^1] performance gain.
However, the Go authors recognize that there are some considerable downsides to passing
arguments in registers: It is more complex has a higher implementation and maintainenance cost
for the compiler. It also makes Go's stack traces—which includes arguments values—really
tricky to implement (since past frame's argument values would be over-written.)
[There's a discussion on github/golang/go.](https://github.com/golang/go/issues/18597)

It's worth noting that Go decided to not make this change and stick with pure stack use.
Except from increased complexity, the reasons were grounded in legacy.

Most programming languages and VMs makes use of registers for arguments and return values
because in practice, even with pure stack calling, registers are clobbered and need to be
saved anyway. An example:

	fun foo(a, b, c int) int { a + c * d }
	fun bar(x, y int) int {
		foo(x + y, x * y, 2) + 10
	}
	fun main -> bar(1, 2)

SSA IR:

	fun foo (int,int,int)->int
		v0 = arg 0      # a
		v1 = arg 1      # b
		v2 = arg 2      # c
		v3 = mul v2 v1  # c * d
		v4 = add v0 v3  # tmp' = a + v3
	ret v4

	fun bar (int,int)->int
		v0 = arg   0      # x
		v1 = arg   1      # y
		v2 = add   v0 v1  # x + y
		v3 = mul   v0 v1  # x * y
		v4 = const 2
		v5 = params v2, v3, v4
		v6 = call foo v5
		v7 = result 0
	ret v7

	fun main
		v0 = const 1
		v1 = const 2
		v2 = params v0, v1
		v3 = call bar v2
	ret

Looking at the main function calling to `bar`, here's what it looks like with arguments and
return values on the stack:

	bar:  # (int,int)->int
		| stack now looks like this:
		|    100-96  1               |  arg "x", 4 bytes
		|    96-92   2               |  arg "y", 4 bytes
		|    92-84   return address  |  8 bytes (64-bit)
		| -- 84 ---- <base pointer> ----------------
		|
		push %rbp            | save stack pointer; store value of rbp to stack
		mov  %rsp, %rbp      | make stack pointer the base pointer
		mov  16(%rbp), %rax  | load "x" argument into rax (84+16 = 100)
		mov  12(%rbp), %rbx  | load "y" argument into rbx (84+12 = 96)
		mov  %rbx, %rcx      | copy "y" so that "add" does not over-write it
		add  %rax, %rcx      | x + y -> rcx
		mul  %rax, %rbx      | x * y -> rbx  (over-writes "y")
		mov  $2    %rax      | store constant 2 in rax (over-writes "x")
		                     | At this point: rcx=v2, rbx=v3, rax=v4
		mov  %rcx, -4(%rbp)  | store v2 on stack
		mov  %rbx, -8(%rbp)  | store v3 on stack
		mov  %rax, -12(%rbp) | store v4 on stack
		call foo
		mov  -12(%rbp), %rax | load result from stack into rax
		add  $10, %rax       | <result> + 10 -> rax
		mov  %rax, 4(%rbp)   | store result value to stack
		ret

	main:
		mov  $1, -4(%rbp)  # store "x" argument on stack
		mov  $2, -8(%rbp)  # store "y" argument on stack
		|
		| stack now looks like this:
		| -- 100 ---- <base pointer> ----------------
		|    100-96  1              |  arg "x", 4 bytes
		|    96-92   2              |  arg "y", 4 bytes
		|
		call bar
		| ignore return value
		ret


Pros & cons, pure stack vs registers:
- (+stack) Simple implementation
- (+stack) Portable
	- In practice assembly and lowered IR is not portable for other reasons.
	- "Portable" here means that the stragegy and code generation does not need
	  to be customized for different machine targets.
- (+regs) Performace
- (+regs) Uses less memory

	main:
		push $1  # store 1 at (rsp), increment rsp
		push $2  # store 1 at (rsp), increment rsp
		call main
		add  $8, %rsp  # move stack pointer back (2*4 = 2*sizeof(int))
		ret

[^1]: [Proposal: Passing Go arguments and results in registers](https://gist.github.com/dr2chase/5a1107998024c76de22e122ed836562d), also referenced in [go review tracker (stale)](https://go-review.googlesource.com/c/proposal/+/35054/)
