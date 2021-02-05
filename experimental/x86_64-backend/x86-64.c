#include "../defs.h"
#include "../memory.h"
#include "../buf.h"


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

