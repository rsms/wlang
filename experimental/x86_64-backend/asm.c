#include "../defs.h"
#include "../os.h"
#include "../ptrmap.h"
#include "asm.h"

#include "elf/elf.h"
#include "elf/builder.h"
#include "elf/file.h"

#include <stdio.h>

/*

Two-pass back-patching assembler
- Pass 1: Layout & symbol table
- Pass 2: Code generation & actual assembly

Pass 1:

  This pass decides the virtual-memory locations for everything.
  - Generate symbols which represents future memory locations.
  - Generate layout for read-only data (constants).
  - Generate layout for writable data (initializers).

  Example:

  ---------range1---------
    foo:
      add ...
      mul ...
      jmp bar
  # bar's address is unknown at this point. Generate symbol1
  ---------range2---------
    bar:                # set address in symbol1
      add ...
      mul ...
      mov $msg_ptr ...
  # data msg_ptr: Generate symbol2 & register data (e.g. a byte array)
  ---------range3---------
      add ...
      jmpz foo           # note: address of foo is already known
      mul ...
  ----------end-----------


Note:
  In the future, if we would support span-dependent instructions, like for example a jump
  that uses different opcode depending on distance, addresses/layout would vary depending on
  future addresses. For this, we could do layout with a linked list of chunks where each
  chunk is a range relative to the previous chunk, separated by variable-length aka unknown
  length instructions.

Q: How is data like string constants stored in the IR?

msg = "Hello world\n"
syscall.write(STDOUT, msg, 12)
--------------------------------------------
b0:
  v0 = InitMem
  v1 = I32Const 1
  v2 = DataPointer <msg>
  v3 = DataSize <msg>
  v4 = PushArg v0 v1
  v5 = PushArg v1 v2
  v6 = PushArg v2 v3
  v7 = Call syscall.write v6
data:
  msg = [readonly] 13xi8 "Hello world\n"

*/

// mini IR

// Linux syscall numbers
// https://github.com/torvalds/linux/blob/v3.13/arch/x86/syscalls/syscall_64.tbl
#define SYSCALL_WRITE 1
#define SYSCALL_EXIT  60

#define STDIN     0
#define STDOUT    1
#define STDERR    2

// x86-64 register encoding (4-bit; from https://wiki.osdev.org/X86-64_Instruction_Encoding)
// Enc         8-bit GP  16-bit GP  32-bit GP  64-bit GP ...
// 0.000 (0)   // AL        AX         EAX        RAX    ...
// 0.001 (1)   // CL        CX         ECX        RCX    ...
// 0.010 (2)   // DL        DX         EDX        RDX    ...
// 0.011 (3)   // BL        BX         EBX        RBX    ...
// 0.100 (4)   // AH, SPL1  SP         ESP        RSP    ...
// 0.101 (5)   // CH, BPL1  BP         EBP        RBP    ...
// 0.110 (6)   // DH, SIL1  SI         ESI        RSI    ...
// 0.111 (7)   // BH, DIL1  DI         EDI        RDI    ...
// 1.000 (8)   // R8L       R8W        R8D        R8     ...
// 1.001 (9)   // R9L       R9W        R9D        R9     ...
// 1.010 (10)  // R10L      R10W       R10D       R10    ...
// 1.011 (11)  // R11L      R11W       R11D       R11    ...
// 1.100 (12)  // R12L      R12W       R12D       R12    ...
// 1.101 (13)  // R13L      R13W       R13D       R13    ...
// 1.110 (14)  // R14L      R14W       R14D       R14    ...
// 1.111 (15)  // R15L      R15W       R15D       R15    ...

// --------------------------------------------------------------
// REX (40-4f) precede opcode or legacy pfx
//   REX = R=64-bit = R EXtension
//   8 additional regs (%r8-%r15), size extensions
//   See section 1.2.7 "REX Prefix" in "AMD64 Architecture Programmer’s Manual, Volume 3".
//
//   REX contains five fields. The upper nibble is unique to the REX prefix and identifies
//   it is as such. The lower nibble is divided into four 1-bit fields (W, R, X, and B)
//
//     7   6   5   4   3   2   1   0
//   +———————————————+———+———+———+———+
//   | 0   1   0   0 | W | R | X | B |
//   +———————————————+———+———+———+———+
//           4
//
//   Note: Not all instructions require REX in 64-bit mode.
//   See table 1-9 "Instructions Not Requiring REX Prefix in 64-Bit Mode" in
//   "AMD64 Architecture Programmer’s Manual, Volume 3".
//
//   REX.W: Operand width (Bit 3). Setting the REX.W bit to 1 specifies a 64-bit operand size.
//   Like the existing 66h operand-size override prefix, the REX 64-bit operand-size override
//   has no effect on byte operations. For non-byte operations, the REX operand-size override
//   takes precedence over the 66h prefix. If a 66h prefix is used together with a REX prefix
//   that has the W bit set to 1, the 66h prefix is ignored. However, if a 66h prefix is used
//   together with a REX prefix that has the W bit cleared to 0, the 66h prefix is not ignored
//   and the operand size becomes 16 bits.
//
//   REX.R: Register field extension (Bit 2). The REX.R bit adds a 1-bit extension
//   (in the most significant bit position) to the ModRM.reg field when that field encodes a
//   GPR, YMM/XMM, control, or debug register. REX.R does not modify ModRM.reg when that field
//   specifies other registers or is used to extend the opcode. REX.R is ignored in such cases.
//
//   REX.X: Index field extension (Bit 1). The REX.X bit adds a 1-bit (msb) extension to the
//   SIB.index field.
//
//   REX.B: Base field extension (Bit 0). The REX.B bit adds a 1-bit (msb) extension to either
//   the ModRM.r/m field to specify a GPR or XMM register, or to the SIB.base field to specify
//   a GPR.
//
//   Note: There are some exceptions (of course there are) to all of this, for certain registers.
//   See table 1-17 "Special REX Encodings for Registers" in "AMD64 Architecture Programmer’s
//   Manual, Volume 3"
//
#define REX_     0b01000000 /* 0x40 */
#define REX_B    0b01000001 /* 0x41 */
#define REX_X    0b01000010 /* 0x42 */
#define REX_XB   0b01000011 /* 0x43 */
#define REX_R    0b01000100 /* 0x44 */
#define REX_RB   0b01000101 /* 0x45 */
#define REX_RX   0b01000110 /* 0x46 */
#define REX_RXB  0b01000111 /* 0x47 */
#define REX_W    0b01001000 /* 0x48 */
#define REX_WB   0b01001001 /* 0x49 */
#define REX_WX   0b01001010 /* 0x4a */
#define REX_WXB  0b01001011 /* 0x4b */
#define REX_WR   0b01001100 /* 0x4c */
#define REX_WRB  0b01001101 /* 0x4d */
#define REX_WRX  0b01001110 /* 0x4e */
#define REX_WRXB 0b01001111 /* 0x4f */
// --------------------------------------------------------------
// ModRM
//   The ModRM byte is optional depending on the instruction.
//   When present, it follows the opcode and is used to specify:
//   - two register-based operands, or
//   - one register-based operand and a second memory-based operand and an addressing mode.
//
//   The ModRM byte is partitioned into three fields: mod, reg, and r/m. Normally the reg field
//   specifies a register-based operand and the mod and r/m fields used together specify a second
//   operand that is either register-based or memory-based. The addressing mode is also specified
//   when the operand is memory-based.
//
//   In 64-bit mode, the REX.R and REX.B bits augment the reg and r/m fields respectively allowing
//   the specification of twice the number of registers.
//
//     7   6   5   4   3   2   1   0
//   +———————+———————————+———————————+
//   |  mod  |    reg    |    r/m    |
//   +———————+———————————+———————————+
//                 |           |
//                 |          REX.B, VEX.B, or XOP.B extend this field to 4 bits
//                 |
//                REX.R, VEX.R or XOP.R extend this field to 4 bits
//
//   The mod field is used with the r/m field to specify the addressing mode for an operand.
//   ModRM.mod = 11b specifies the register-direct addressing mode. In the register-direct mode,
//   the operand is held in the specified register.
//   ModRM.mod values less than 11b specify register-indirect addressing modes.
//   In register-indirect addressing modes, values held in registers along with an optional
//   displacement specified in the instruction encoding are used to calculate the address of a
//   memory-based operand.
//
//   The reg field is used to specify a register-based operand, although for some instructions,
//   this field is used to extend the operation encoding.
//
//   The r/m field is used in combination with the mod field to encode 32 different operand
//   specifications.
//
//   reg bits   Register
//   000 (0)    rAX, MMX0, XMM0, YMM0
//   001 (1)    rCX, MMX...
//   010 (2)    rDX, MMX...
//   011 (3)    rBX, MMX...
//   100 (4)    rSP, MMX... Note: Indexed register-indirect addressing "SIB" when r/m!=11b
//   101 (5)    rBP, MMX...
//   110 (6)    rSI, MMX...
//   111 (7)    rDI, MMX...
//
// See Section 1.4 "ModRM and SIB Bytes" in "AMD64 Architecture Programmer’s Manual, Volume 3".
//
typedef enum {
  Mod_OFS0   = 0x00, Mod_OFS8   = 0x40, Mod_OFS32  = 0x80, Mod_REG    = 0xc0,
  Mod_SCALE1 = 0x00, Mod_SCALE2 = 0x40, Mod_SCALE4 = 0x80, Mod_SCALE8 = 0xc0,
  Mod_MASK   = 0xc0
} x86Mode;
#define ModRM(mode, r1, r2)  ((u8)((/*x86Mode*/mode)+(((r1)&7)<<3)+((r2)&7)))

// macros for constructing variable-length opcodes. -(len+1) is in LSB.
#define VO(o)  ((u32)(0x0000fe + (0x##o<<24)))

// x86 opcodes. Name codes: r=reg, i=imm, b=8bit, m=mode(uses ModRM)
typedef enum X86op {
  // fixed-length, 1 byte
  O_CALL   = 0xe8,
  O_JMP    = 0xe9,
  O_LEA    = 0x8d, // load effective address
  O_INT    = 0xcd, // interrupt
  O_INT3   = 0xcc, // breakpoint
  O_MOVrib = 0xb0, // really b0+r  e.g. b301=mov,$1,%bl
  O_MOVri  = 0xb8, // really b8+r  e.g. b801000000=mov,$1,%eax; bb01000000=mov,$1,%ebx
  O_MOVmi  = 0xc7, // move with 16- or 32-bit immediate

  // fixed-length, 2 bytes (little endian!)
  O_SYSCALL = 0x050f,

  // variable-length
  O_V_OR  = VO(0b),
  O_V_MOV = VO(8b),
  //O_V_MOVmi = VO(c7), // move with 16- or 32-bit immediate
} X86op;


typedef enum Reg {
  //          Enc         x86 name
  // Special registers
  R_SP = 4,  // 0.100 (4)   SP
  R_BP = 5,  // 0.101 (5)   BP
  // General-purpose integer registers
  R_AX = 0,  // 0.000 (0)   AX
  R_CX = 1,  // 0.001 (1)   CX
  R_DX = 2,  // 0.010 (2)   DX
  R_BX = 3,  // 0.011 (3)   BX
  R_SI = 6,  // 0.110 (6)   SI
  R_DI = 7,  // 0.111 (7)   DI
  R_8  = 8,  // 1.000 (8)   R8
  R_9  = 9,  // 1.001 (9)   R9
  R_10 = 10, // 1.010 (10)  R10
  R_11 = 11, // 1.011 (11)  R11
  R_12 = 12, // 1.100 (12)  R12
  R_13 = 13, // 1.101 (13)  R13
  R_14 = 14, // 1.110 (14)  R14
  R_15 = 15, // 1.111 (15)  R15
} Reg;

// typedef enum Op { O_MOVQ, O_MOVL, O_INT, O_DATPTR, O_DATSIZE } Op;
// typedef struct Instr { Op op; u64 arg1, arg2; } Instr;
// typedef struct Data { const void* ptr; size_t size; } Data;
// typedef struct Prog {
//   const Instr* instrv; size_t instrc;
//   const Data* datav;   size_t datac;
// } Prog;
// static const Data helloworld_data[] = {
//   { .ptr = (const void*)"Hello world\n", .size = 13 },
// };
// static const Instr helloworld_instrs[] = {
//   // syscall.write(STDOUT, msg, len(msg))
//   { O_MOVQ, SYS_WRITE, R_RAX },
//   { O_MOVQ, STDOUT, R_RBX },
//   { O_DATPTR,  /* data index */0, R_RCX },
//   { O_DATSIZE, /* data index */0, R_RDX },
//   { O_INT, SYSCALL, 0 },
//   // syscall.exit(0)
//   { O_MOVL, SYS_EXIT, R_EAX },
//   { O_MOVL, 0, R_EBX },
//   { O_INT, SYSCALL, 0 },
// };
// static const Prog prog_helloworld_mini2 = {
//   .instrv = helloworld_instrs, .instrc = sizeof(helloworld_instrs)/sizeof(Instr),
//   .datav  = helloworld_data,   .datac  = sizeof(helloworld_data)/sizeof(Data),
// };


#define BUFGROW   if ((buf)->cap <= (buf)->len) { BufMakeRoomFor((buf), 4096); }
#define PUSHBYTE(b)  ({ buf->ptr[buf->len++] = b; })
#define PUSHU16(v)   ({ *((u16*)&buf->ptr[buf->len]) = v; buf->len += 2; })
#define PUSHU32(v)   ({ *((u32*)&buf->ptr[buf->len]) = v; buf->len += 4; })
#define PUSHU64(v)   ({ *((u64*)&buf->ptr[buf->len]) = v; buf->len += 8; })

// i32 -> reg<i64>
static void emit_mov64_imm32(Buf* buf, Reg dstreg, u32 imm) {
  PUSHBYTE( REX_W );
  PUSHBYTE( O_MOVmi );
  PUSHBYTE( ModRM(Mod_REG, 0, dstreg) );
  PUSHU32( imm );
}

// i64 -> reg<i64>
static void emit_mov64_imm64(Buf* buf, Reg dstreg, u64 imm) {
  PUSHBYTE( REX_W );
  PUSHBYTE( O_MOVri + dstreg );
  PUSHU64( imm );
}

// i32 -> reg<i32>
static void emit_mov32_imm32(Buf* buf, Reg dstreg, u32 imm) {
  PUSHBYTE( O_MOVri + dstreg );
  PUSHU32( imm );
}

// i8 -> reg<i8>
static void emit_mov8_imm8(Buf* buf, Reg dstreg, u8 imm) {
  PUSHBYTE( O_MOVrib + dstreg );
  PUSHBYTE( imm );
}

// static void emit_mov_reg(Buf* buf, Reg dstreg, Reg srcreg) {}

inline static void emit_syscall(Buf* buf) {
  // Documented in the x86-64 ABI in section A.2.1:
  //   User-level applications use as integer registers for passing the sequence
  //   %rdi, %rsi, %rdx, %rcx, %r8 and %r9.
  //   The kernel interface uses %rdi, %rsi, %rdx, %r10, %r8 and %r9.
  //   A system-call is done via the syscall instruction.
  //   The kernel destroys registers %rcx and %r11.
  //   The number of the syscall has to be passed in register %rax.
  //   System-calls are limited to six arguments,no argument is passed directly on the stack.
  //   Returning from the syscall, register %rax contains the result of the system-call.
  //   A value in the range between -4095 and -1 indicates an error, it is -errno.
  //   Only values of class INTEGER or class MEMORY are passed to the kernel.
  PUSHU16( O_SYSCALL );
}


typedef struct Data {
  const void* ptr;
  size_t      size;
  bool        readonly;
} Data;

typedef struct Ref {
  u64         offs;
  const void* id;
} Ref;

// Define an Array type of Ref
DefArrayBuffer(RefArray, Ref);
//  void RefArrayInit(RefArray* nonull a, Memory nullable mem, size_t cap);
//  void RefArrayFree(RefArray* nonull a);
//  Ref* RefArrayAt(RefArray* nonull a, size_t index);
//  void RefArrayPush(RefArray* nonull a, Ref v);
//  Ref  RefArrayPop(RefArray* nonull a);
//  Ref* RefArrayAlloc(RefArray* nonull a, size_t count);
//  void RefArrayMakeRoomFor(RefArray* nonull a, size_t count);
#define RefArrayForEach(a, LOCALNAME) ArrayBufferForEach(a, Ref, LOCALNAME)

typedef struct Asm {
  Buf      buf;
  RefArray unresolved;  // [Ref]
  PtrMap   resolved;
} Asm;


static void AsmRegUnresolved(Asm* a, const void* id, u64 textoffs) {
  // register unresolved memory location
  auto ref = RefArrayAlloc(&a->unresolved, 1);
  ref->offs = textoffs;
  ref->id = id;
}


static u64 AsmAddrof(Asm* a, const void* id, u64 textoffs) {
  auto addr = (u64)PtrMapGet(&a->resolved, (void*)id);
  if (addr == 0) {
    AsmRegUnresolved(a, id, textoffs);
  }
  return addr;
}


static void dumpResolved(const void* key, void* value, bool* stop, void* userdata) {
  dlog("  %p => VMA %016llx", key, (u64)value);
}


static void gen_prog2(Memory mem, Buf* textbuf, Buf* rodatabuf, u64 vmastart) {
  Asm _a = {}; auto a = &_a;
  RefArrayInit(&a->unresolved, mem, 16);
  PtrMapInit(&a->resolved, 32, mem);

  // list of data to generate. Instead of appending to rodatabuf as we codegen, add IR values
  // with constant data to this list instead to maximize cache efficiency.
  Array datalist; void* datalist_storage[32];
  ArrayInitWithStorage(&datalist, datalist_storage, countof(datalist_storage));

  // virtual memory address start
  u64 textVMA = vmastart;

  // data. (Eventually sourced from in IRValue.aux)
  auto msg1 = "Hello world\n";

  // codegen
  auto buf = textbuf;
  BUFGROW
  // syscall.write(STDOUT, &msg, len(msg))
  emit_mov64_imm32(buf, R_AX, SYSCALL_WRITE);
  emit_mov64_imm32(buf, R_DI, STDOUT);             // syscall.write arg0: fd
  emit_mov64_imm64(buf, R_SI, 0x0000000000000000); // syscall.write arg1: ptr
    AsmRegUnresolved(a, msg1, buf->len - 8);
    ArrayPush(&datalist, msg1, mem); // TODO: IRValue instead of const char*
  emit_mov64_imm32(buf, R_DX, strlen(msg1));       // syscall.write arg2: size
  emit_syscall(buf);
  // syscall.exit(42)
  emit_mov64_imm32(buf, R_AX, SYSCALL_EXIT);
  emit_mov64_imm32(buf, R_DI, 42);  // syscall.exit arg0: exit status 42
  emit_syscall(buf);
  // end codegen

  // data
  u64 rodataVMA = align2(textVMA + textbuf->len, 4);
  ArrayForEach(&datalist, const char, str) {
    PtrMapSet(&a->resolved, str, (void*)(u64)(rodataVMA + rodatabuf->len));
    BufAppend(rodatabuf, str, strlen(str) + 1);
  }
  ArrayFree(&datalist, mem);

  // patch unresolved addresses in .text
  dlog("unresolved:");
  RefArrayForEach(&a->unresolved, r) {
    dlog("  offs=%llu, id=%p", r->offs, r->id);
    auto addr = (u64)PtrMapGet(&a->resolved, r->id);
    if (addr != 0) {
      dlog("    resolve => %016llx", addr);
      *((u64*)&textbuf->ptr[r->offs]) = addr;
    } else {
      dlog("    unresolved!");
    }
  }

  dlog("resolved:");
  PtrMapIter(&a->resolved, dumpResolved, NULL);
}


static void gen_prog1(Buf* buf) {
  BUFGROW
  // syscall.write(STDOUT, &msg, len(msg))
  emit_mov64_imm32(buf, R_AX, SYSCALL_WRITE);
  emit_mov64_imm32(buf, R_DI, STDOUT);             // syscall.write arg0: fd
  emit_mov64_imm64(buf, R_SI, 0x00000000004000ac); // syscall.write arg1: ptr
  emit_mov64_imm32(buf, R_DX, 12);                 // syscall.write arg2: size
  emit_syscall(buf);
  // syscall.exit(42)
  emit_mov64_imm32(buf, R_AX, SYSCALL_EXIT);
  emit_mov64_imm32(buf, R_DI, 42);  // arg0: exit status 42
  emit_syscall(buf);

  // auto endaddr = align2(0x0000000000400078 + buf->len, 4);
  // dlog("textlen: %zu, endaddr: %016x", buf->len, endaddr);
}


static const char* prog_helloworld_mini = ""
  // 48   prefix  REX_W
  // c7   op      O_MOVmi
  // c0   mode    ModRM(Mod_REG, 0, R_AX=0)
  // rest 32-bit immediate 0x00000001
  "48 c7 c0 01 00 00 00" // movq $SYSCALL_WRITE, %rax

  // 48   prefix  REX_W
  // c7   op      O_MOVmi
  // c7   mode    ModRM(Mod_REG, 0, R_DI=7)
  // rest 32-bit immediate 0x00000001
  "48 c7 c7 01 00 00 00" // movq $STDOUT, %rdi

  // 48   prefix  REX_W
  // be   op      O_MOVri+R_SI (0xb8 + 6)
  // rest 64-bit immediate 0x00000000004000ac
  "48 be ac 00 40 00 00 00 00 00 " // movabsq $hellomsg, %rsi
    // buffer pointer (becomes VMA addr into .rodata)

  // 48   prefix  REX_W
  // c7   op      O_MOVmi
  // c2   mode    ModRM(Mod_REG, 0, R_DX=2)
  // rest 32-bit immediate 0x0000000c
  "48 c7 c2 0c 00 00 00 " // movq $12, %rdx (12=0x0c)

  // 0f05 op      Syscall
  "0f 05 "

  // 48   prefix  REX_W
  // c7   op      O_MOVmi
  // c0   mode    ModRM(Mod_REG, 0, R_AX=0)
  // rest 32-bit immediate 0x0000003c
  "48 c7 c0 3c 00 00 00 " // movl $SYSCALL_EXIT, %eax

  // 48   prefix  REX_W
  // c7   op      O_MOVmi
  // c7   mode    ModRM(Mod_REG, 0, R_DI=7)
  // rest 32-bit immediate 0x0000002a
  "48 c7 c7 2a 00 00 00 " // movq $42, %rdi  # exit status

  // 0f05 op      Syscall
  "0f 05 "
;


// simply exits with status "42"
static const char* prog_minimal = ""
  "bb 2a 00 00 00 " // movl  $42, %ebx
  "b8 01 00 00 00 " // movl  $1, %eax
  "cd 80          " // int $128
;


static void appendprog(Buf* buf, const char* prog) {
  // appends each hexadecimal byte found in c-string prog.
  // all non-hex chars are ignored.
  size_t len = strlen(prog);
  u8 val = 0;
  int n = 0;
  for (size_t i = 0; i < len; i++) {
    u8 c = (u8)prog[i];
    if (c >= '0' && c <= '9') {        c = c - '0';
    } else if (c >= 'A' && c <= 'F') { c = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') { c = c - 'a' + 10;
    } else { continue; }
    if (n == 0) {
      val = c;
      n++;
    } else {
      n = 0;
      BufAppendc(buf, (val << 4) | c);
    }
  }
}


typedef struct ELF64 {
  Buf    buf;        // Main buffer (ELF header + program headers + data segments)
  u16    phnum;      // number of program headers (in buf + sizeof(ELH header))
  Buf    shbuf;      // section headers
  Buf    strtab;     // string table
  Buf    shstrtab;   // section header string table
  Buf    symtab;     // symbol table
} ELF64;

static void ELF64Init(ELF64* e, Memory nullable mem) {
  memset(e, 0, sizeof(ELF64));
  // main buffer (ELF header + program headers + data segments)
  BufInit(&e->buf, mem, 4096);
  memset(e->buf.ptr, 0, sizeof(Elf64_Ehdr)); // reserve space for ELF header
  e->buf.len += sizeof(Elf64_Ehdr);

  // section headers buffer. Section header #0 is the "null"-section header.
  BufInit(&e->shbuf, mem, sizeof(Elf64_Shdr) * 6); // usually at least 6 sections
  memset(e->shbuf.ptr, 0, sizeof(Elf64_Shdr)); // allocate the "null"-section header.
  e->shbuf.len += sizeof(Elf64_Shdr);

  // strtab; string table. starts with \0. Usually just the zero name and "_start".
  BufInit(&e->strtab, mem, 16);
  e->strtab.ptr[0] = 0; // allocate the "null" string
  e->strtab.len++;

  // shstrtab; section header string table. starts with \0
  // Usually: "", ".text", ".rodata", ".symtab", ".strtab" and ".shstrtab".
  BufInit(&e->shstrtab, mem, 64);
  e->shstrtab.ptr[0] = 0; // allocate the "null" string
  e->shstrtab.len++;

  // symtab. usually just the "null" symbol and the "_start" symbol.
  BufInit(&e->symtab, mem, sizeof(Elf64_Sym) * 4);
  memset(e->symtab.ptr, 0, sizeof(Elf64_Sym)); // allocate the "null" symbol
  e->symtab.len += sizeof(Elf64_Sym);
}

inline static Memory* ELF64Memory(ELF64* e) {
  return e->buf.mem;
}

inline static Elf64_Ehdr* ELF64GetEH(ELF64* e) {
  return (Elf64_Ehdr*)e->buf.ptr;
}

static u32 strtabAdd(Buf* buf, const char* pch, size_t size) {
  if (size == 0) {
    return 0; // return the null string index
  }
  if (((u64)buf->len) + ((u64)size) >= 0xFFFFFFFF) {
    return 0; // table is full; return the null string index
  }
  u32 offs = (u32)buf->len;

  // append pch of size + a sentinel null byte
  BufMakeRoomFor(buf, size + 1);
  memcpy(buf->ptr + buf->len, pch, size);
  buf->ptr[buf->len + size] = 0;
  buf->len += size + 1;

  return offs;
}

// ELF64AddStr adds a string to strtab. Returns the strtab index.
inline static u32 ELF64AddStr(ELF64* e, const char* s) {
  return strtabAdd(&e->strtab, s, strlen(s));
}

// ELF64AddSym adds a symbol to the symbol table
// - shndx  section index of owning section (ELF_SHN_UNDEF for "no section")
// - bind   a ELF_STB_* constant
// - typ    a ELF_STT_* constant
// Returned pointer is valid until the next call to ELF64AddSym.
// You usually want to set st_value (and sometimes st_size) on the returned symbol.
static Elf64_Sym* ELF64AddSym(ELF64* e, const char* name, u16 shndx, u8 bind, u8 typ) {
  auto sym = (Elf64_Sym*)BufAllocz(&e->symtab, sizeof(Elf64_Sym));
  sym->st_name = strtabAdd(&e->strtab, name, strlen(name));
  sym->st_info = ELF_ST_INFO(bind, typ);
  sym->st_shndx = shndx;
  return sym;
}

// ELF64AddPH adds a program header.
// Returns program header index which can be used with ELF64GetPH to access the struct.
static u16 ELF64AddPH(ELF64* e) {
  u16 phidx = e->phnum;
  e->phnum++;
  BufAllocz(&e->buf, sizeof(Elf64_Phdr));
  return phidx;
}

inline static Elf64_Phdr* ELF64GetPH(ELF64* e, u16 phidx) {
  assert(phidx < e->phnum);
  return (Elf64_Phdr*)&e->buf.ptr[sizeof(Elf64_Ehdr) + ((size_t)phidx * sizeof(Elf64_Phdr))];
}


// ELF64AddSection adds a new section header.
// Affects e->shbuf, e->shstrtab
static Elf64_Shdr* ELF64AddSection(ELF64* e, const char* name) {
  auto sh = (Elf64_Shdr*)BufAllocz(&e->shbuf, sizeof(Elf64_Shdr));
  sh->sh_name = strtabAdd(&e->shstrtab, name, strlen(name));
  return sh;
}

// ELF64StartSection starts a new section which is assumed to have data.
// If you want a section that doesn't point to data, use ELF64AddSection.
// returned pointer only valid until next call to ELF64StartSection.
// Affects e->shbuf, e->shstrtab, e->buf
static Elf64_Shdr* ELF64StartSection(ELF64* e, const char* name, u64 sh_addralign) {
  auto align = align2(e->buf.len, sh_addralign);
  if (align != e->buf.len) {
    // add padding to force alignment
    BufAllocz(&e->buf, align - e->buf.len);
  }
  auto sh = ELF64AddSection(e, name);
  sh->sh_offset = e->buf.len; // section's data file offset
  sh->sh_addralign = sh_addralign;
  return sh;
}


static int _symcmp64(const void* ptr1, const void* ptr2) {
  auto ainfo = ((const Elf64_Sym*)ptr1)->st_info;
  auto binfo = ((const Elf64_Sym*)ptr2)->st_info;
  if (ELF_ST_BIND(ainfo) == ELF_STB_LOCAL) {
    if (ELF_ST_BIND(binfo) != ELF_STB_LOCAL) {
      return -1; // a is local, b is global
    }
    return 0; // both are local
  } if (ELF_ST_BIND(binfo) == ELF_STB_LOCAL) {
    return 1;  // a is global, b is local
  }
  return 0;  // both are global
}

// sort symbols of SYMTYPE in buf
// void SYMTAB64_SORT(const Buf* buf)
#define SYMTAB64_SORT(buf) \
  qsort((buf)->ptr, (buf)->len / sizeof(Elf64_Sym), sizeof(Elf64_Sym), &_symcmp64)

// void SYMTAB_FOR_EACH(const Buf* buf, Elf32_Sym|Elf64_Sym, NAME)
#define SYMTAB_FOR_EACH(buf, SYMTYPE, LOCALNAME) \
  for ( \
    auto LOCALNAME        = (SYMTYPE*)(buf)->ptr, \
         LOCALNAME##__end = (SYMTYPE*)&(buf)->ptr[(buf)->len]; \
    LOCALNAME < LOCALNAME##__end; \
    LOCALNAME++ \
  ) /* <body should follow here> */


static Elf64_Ehdr* finalizeEH(ELF64* e, u8 encoding) {
  auto eh = (Elf64_Ehdr*)e->buf.ptr;
  *((u32*)&eh->e_ident[0]) = *((u32*)&"\177ELF");
  eh->e_ident[ELF_EI_CLASS]   = ELF_CLASS_64;
  eh->e_ident[ELF_EI_DATA]    = encoding;
  eh->e_ident[ELF_EI_VERSION] = ELF_V_CURRENT;
  eh->e_ident[ELF_EI_OSABI]   = ELF_OSABI_NONE;
  eh->e_type                  = ELF_FT_EXEC;
  eh->e_version               = ELF_V_CURRENT;
  eh->e_flags     = 0;                   // Processor-specific flags. Usually 0.
  eh->e_ehsize    = sizeof(Elf64_Ehdr);  // ELF header's size in bytes
  eh->e_phentsize = sizeof(Elf64_Phdr);  // size of a program header
  eh->e_shentsize = sizeof(Elf64_Shdr);  // size of a section header
  return eh;
}

static void ELF64Finalize(ELF64* e, u8 encoding, u16 e_machine, u64 entryaddr) {
  // Note: If we were to write directly to a file, we could replace the BufAppend
  // calls here with file-write calls.
  //
  // When this function is called, e->buf is expected to look like this:
  //
  //    +————————————————————————————+
  //    | ELF header                 |
  //    +————————————————————————————+
  //    | Program headers            |
  //    | ...                        |
  //    +————————————————————————————+
  //    | TEXT, DATA etc segments    |
  //    | ...                        |
  //
  // This function will append the following:
  //
  //    | .symtab segment            |
  //    +————————————————————————————+
  //    | .strtab segment            |
  //    +————————————————————————————+
  //    | .shstrtab segment          |
  //    +————————————————————————————+
  //    | section headers            |
  //    | ...                        |
  //    +————————————————————————————+
  //
  // Finally the ELF header is patched with information now known about section headers,
  // program headers, virtual memory addresses, and segments.
  //

  size_t shnum = e->shbuf.len / sizeof(Elf64_Shdr); // number of section headers
  //u32 strtab_sec_index = shnum + 1;

  // .symtab section
  auto sh = ELF64StartSection(e, ".symtab", /* sh_addralign */ 8);
  sh->sh_type = ELF_SHT_SYMTAB;
  sh->sh_size = e->symtab.len;
  sh->sh_link = shnum + 1; // section index of .strtab
  sh->sh_entsize = sizeof(Elf64_Sym);
  // Sort symbols so that local bindings appear first: (a requirement of ELF)
  SYMTAB64_SORT(&e->symtab);
  u32 nlocals = 0;
  SYMTAB_FOR_EACH(&e->symtab, Elf64_Sym, sym) {
    if (ELF_ST_BIND(sym->st_info) != ELF_STB_LOCAL) {
      break;
    }
    nlocals++;
  }
  // for symtab sections, sh_info should hold one greater than the symbol table index of
  // the last local symbol.
  sh->sh_info = nlocals;
  BufAppend(&e->buf, e->symtab.ptr, e->symtab.len);

  // .strtab section
  sh = ELF64StartSection(e, ".strtab", /* sh_addralign */ 1);
  sh->sh_type = ELF_SHT_STRTAB;
  sh->sh_size = e->strtab.len;
  BufAppend(&e->buf, e->strtab.ptr, e->strtab.len);

  // .shstrtab section
  sh = ELF64StartSection(e, ".shstrtab", /* sh_addralign */ 1);
  sh->sh_type = ELF_SHT_STRTAB;
  sh->sh_size = e->shstrtab.len;
  BufAppend(&e->buf, e->shstrtab.ptr, e->shstrtab.len);

  // write section headers, saveing section header table offset for use later in PH
  u64 e_shoff = e->buf.len;
  BufAppend(&e->buf, e->shbuf.ptr, e->shbuf.len);

  // update section header count (after adding standard sections)
  shnum = e->shbuf.len / sizeof(Elf64_Shdr);
  assert(shnum <= 0xFFFF); // must fit in u16

  // update ELF header
  auto eh = finalizeEH(e, encoding);
  eh->e_machine  = e_machine;
  eh->e_phoff    = sizeof(Elf64_Ehdr); // Program header table file offset (follows ELF header)
  eh->e_phnum    = e->phnum;   // number of program headers
  eh->e_shoff    = e_shoff;    // Section header table file offset
  eh->e_shnum    = (u16)shnum; // number of section headers
  eh->e_entry    = entryaddr;  // Entry point virtual address
  eh->e_shstrndx = shnum - 1;  // section header string stable section index (last section)
}


static void regUnresolved(RefArray* a, const void* id, u64 textoffs) {
  // register unresolved memory location
  auto ref = RefArrayAlloc(a, 1);
  ref->offs = textoffs;
  ref->id = id;
}


/*
Notes on section order

.text and .rodata are colocated as they share the same program header (both read-only.)
The question is: which subsegment comes first? .rodata or .text?

Clang always places .text first, then .rodata. My guess is that Clang does this so that in the
case you need a lot of zeroed read-only memory, you can set p_memsz to a larger value than
p_filesz and the ELF loader will allocate zeroed memory AT THE END of the segment, which would
be at the end of .rodata. If we placed .text at the end, then the additional zeroed memory
would be at the end of .text, not .rodata.

Now, .rodata and .text are just concepts. In practice the .rodata bytes and .text bytes
are all in the same segment and thus could be mixed at will. BUT. Mixing is a very bad idea
because the data in .text is loaded as instructions and I think it's safe to assume that
CPUs and OSes makes assumptions about .text being dense i.e. for reasons of prefetch, cache,
etc.

We could have multiple rodata sections. In the case we need a bunch of read-only zero data,
we can place that in a "second .rodata" section after .text.

The main upside of placing .rodata ahead of .text is that we can use a simpler implementation
of the assembler where we don't have to track & backpatch VMAs of data in the text section.

Example: print("Hello") has a call plus a piece of constant read-only data (a string.)
The generated code might look something like this:

  .text
  00400078  ...
  00400090  MOV GetElementPointer(data1) -> rCX  // what is the address of data1?
  0040009c  ...
  .rodata
  00400100  data1: "Hello"  // address is now known. packpatch code above

If we instead place .rodata first, the address of data will be known as we generate the code:

  .rodata
  00400078  data1: "Hello"
  .text
  00400090  ...
  004000a8  MOV GetElementPointer(data1) -> rCX  // address is 00400078
  004000b4  ...

*/

/*

## Code organization

assembler puts code into a box
~~~~~~~~~      ~~~~        ~~~

assembler
  creates a new box (ELF)
  calls gen_code to make code in box
  seals box

*/


// returns the VMA of the text section (segment entry address)
static u64 gen64ROExeSegment(ELF64* e, u64 vma, u16 phidx) {
  auto mem = ELF64Memory(e);

  // start .rodata section
  auto sh = ELF64StartSection(e, ".rodata", /*sh_addralign*/ 8);
  //u64 rodataoffs = sh->sh_offset;
  u64 rodatavma = vma + sh->sh_offset;
  // vma += e->buf.len; // offset VMA with header size
  sh->sh_addr  = rodatavma;
  sh->sh_type  = ELF_SHT_PROGBITS;
  sh->sh_flags = ELF_SHF_ALLOC;

  u64 symvma = rodatavma;
  PtrMap resolved; PtrMapInit(&resolved, 32, mem);

  auto msg1 = "Hello world\n";
  u64 msg1vma = symvma;
  PtrMapSet(&resolved, msg1, (void*)symvma);
  size_t datasize = strlen(msg1) + 1;
  BufAppend(&e->buf, msg1, datasize);
  // add local symbol
  auto sym = ELF64AddSym(e, "msg1", /*shndx=.text*/1, ELF_STB_LOCAL, ELF_STT_OBJECT);
  sym->st_value = symvma;
  symvma += datasize; // TODO: alignment for data that needs it

  // set rodata section size
  sh->sh_size = e->buf.len - sh->sh_offset;


  // start .text section
  sh = ELF64StartSection(e, ".text", /*sh_addralign*/ 8);
  u64 textvma = vma + sh->sh_offset;   // VMA of TEXT segment and program entry point
  sh->sh_addr  = textvma;
  sh->sh_type  = ELF_SHT_PROGBITS;
  sh->sh_flags = ELF_SHF_ALLOC | ELF_SHF_EXECINSTR;
  dlog("[gen64] .text start at VMA %08llx", textvma);

  // add local section symbol for .text to symtab
  auto secsym = ELF64AddSym(e, "", /*shndx=.text*/2, ELF_STB_LOCAL, ELF_STT_SECTION);
  secsym->st_value = textvma;
  // add exported "_start" symbol to symtab
  auto startsym = ELF64AddSym(e, "_start", /*shndx=.text*/2, ELF_STB_GLOBAL, ELF_STT_FUNC);
  startsym->st_value = textvma;

  // .text
  auto buf = &e->buf;
  BUFGROW

  // "unresolved" holds a list of references to unresolved VMAs (offsets in e->buf)
  RefArray unresolved; RefArrayInit(&unresolved, mem, 16);

  // TODO: AddLabel(vma + e->buf.len, (const void*)id);
  // TODO: AddLabelExport(vma + e->buf.len, (const void*)id, "_start"); // adds to symtab
  // syscall.write(STDOUT, &msg, len(msg))
  emit_mov64_imm32(buf, R_AX, SYSCALL_WRITE);
  emit_mov64_imm32(buf, R_DI, STDOUT);             // syscall.write arg0: fd
  emit_mov64_imm64(buf, R_SI, msg1vma);            // syscall.write arg1: ptr
  emit_mov64_imm32(buf, R_DX, strlen(msg1));       // syscall.write arg2: size
  emit_syscall(buf);
  // syscall.exit(42)
  emit_mov64_imm32(buf, R_AX, SYSCALL_EXIT);
  emit_mov64_imm32(buf, R_DI, 42);  // syscall.exit arg0: exit status 42
  emit_syscall(buf);
  // end codegen

  // set text section size
  sh->sh_size = e->buf.len - sh->sh_offset;

  // patch unresolved addresses in .text
  dlog("unresolved:");
  RefArrayForEach(&unresolved, r) {
    dlog("  offs=%llu, id=%p", r->offs, r->id);
    auto addr = (u64)PtrMapGet(&resolved, r->id);
    if (addr != 0) {
      dlog("    resolve => %016llx", addr);
      *((u64*)&buf->ptr[r->offs]) = addr;
    } else {
      dlog("    unresolved!");
    }
  }
  dlog("resolved:");
  PtrMapIter(&resolved, dumpResolved, NULL);

  return textvma;
}


static void gen64() {
  ELF64 _e = {}; auto e = &_e;
  ELF64Init(e, NULL);

  // virtual memory address base
  const u64 vma = 0x400000;

  // add a "LOAD" program header; describing a segment
  auto rxseg = ELF64AddPH(e);
  // Note: no more program headers can be added once we start writing data to e->buf.

  // generate read-only executable segment
  auto textvma = gen64ROExeSegment(e, vma, rxseg);

  // patch our main read-only executable program header
  auto ph = ELF64GetPH(e, rxseg);
  u64 vmaoffs  = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr); // TODO FIXME hard coded
  ph->p_type   = ELF_PT_LOAD;
  ph->p_flags  = ELF_PF_R|ELF_PF_X;
  ph->p_vaddr  = ph->p_paddr = vma + vmaoffs;
  ph->p_align  = 0x200000; // 2^21
  ph->p_offset = vmaoffs;            // Segment start offset in file
  ph->p_filesz = e->buf.len - vmaoffs;   // Segment size in file
  ph->p_memsz  = ph->p_filesz; // Segment size in memory (can be larger than p_filesz)

  // finalize ELF file
  ELF64Finalize(e, ELF_DATA_2LSB, ELF_M_X86_64, textvma);

  // PtrMapFree(&resolved);
  // RefArrayFree(&unresolved);

  // write to file
  if (!os_writefile("./thingy2.elf", e->buf.ptr, e->buf.len)) {
    perror("os_writefile");
  }
}




void AsmELF() {


  // make with new functions
  gen64();

  // // inspect
  // ELFFile _f; auto f = &_f;
  // ELFFileInit(f, "./thingy.elf", buf.ptr, buf.len);
  // if (ELFFileValidate(f, stderr)) {
  //   ELFFilePrint(f, stdout);
  // }

  { const char* filename = "./thingy2.elf";
    dlog("----------------------------------------------------------------\n%s", filename);
    size_t len = 0;
    u8* ptr = os_readfile(filename, &len, NULL);
    ELFFile _f; auto f = &_f;
    ELFFileInit(f, filename, ptr, len);
    if (ELFFileValidate(f, stderr)) {
      ELFFilePrint(f, stdout);
    }
    memfree(NULL, ptr);
  }
  // { const char* filename = "./misc/min-docker/mini2.linux";
  //   dlog("----------------------------------------------------------------\n%s", filename);
  //   size_t len = 0;
  //   u8* ptr = os_readfile(filename, &len, NULL);
  //   ELFFile _f; auto f = &_f;
  //   ELFFileInit(f, filename, ptr, len);
  //   if (ELFFileValidate(f, stderr)) {
  //     ELFFilePrint(f, stdout);
  //   }
  //   memfree(NULL, ptr);
  // }
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

// auto file = "./thingy.elf";
// if (!os_writefile(file, buf, len)) {
//   perror("os_writefile");
// }
