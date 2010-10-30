/*
 * test relax
 * lw <-> lwp!  : rs = r2, offset & 0x3 == 0, offset >> 2 : 5b
 * lh <-> lhp!  : rs = r2, offset & 0x1 == 0, offset >> 1 : 5b
 * lbu <-> lbu! : rs = r2, offset != 0, offset : 5b
 * sw <-> swp!  : rs = r2, offset & 0x3 == 0, offset >> 2 : 5b
 * sh <-> shp!  : rs = r2, offset & 0x1 == 0, offset >> 1 : 5b
 * sb <-> sb!   : rs = r2, offset != 0, offset : 5b

 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16, shift
.align 4

  \insn32 r3, [r2, 0x4 << \shift]     #32b -> 16b
  \insn16 r3, 0x4 << \shift

  \insn32 r4, [r2, 0xC << \shift]      #32b -> 16b
  \insn16 r4, 0xC << \shift

  \insn32 r7, [r2, 0x12 << \shift]     #32b -> 16b
  \insn32 r7, [r2, 0x12 << \shift]     #32b -> 16b

  \insn16 r8, 0x8 << \shift
  \insn32 r8, [r2, 0x8 << \shift]      #32b -> 16b

  \insn32 r5, [r2, 0x20 << \shift]     #No transform
  \insn32 r5, [r2, 0x20 << \shift]     #No transform

  \insn32 r6, [r6, 0x8 << \shift]      #No transform
  \insn32 r6, [r6, 0x8 << \shift]      #No transform

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16, shift
.align 4

  \insn16 r0, 0xC                      #16b -> 32b
  \insn32 r0, [r5, 0xFF]       

  \insn16 r15, 0x0                     #16b -> 32b
  \insn32 r15, [r4, 0xFF]       
 
  \insn16 r4, 0x8                      #No transform
  \insn16 r4, 0x8                      #No transform

  \insn16 r7, 0x8                      #No transform
  \insn32 r7, [r2, 0x8 << \shift]

.endm

  tran3216 "lw", "lwp!", 2
  tran3216 "lh", "lhp!", 1
  tran3216 "lbu", "lbup!", 0
  tran3216 "sw", "swp!", 2
  tran3216 "sh", "shp!", 1
  tran3216 "sb", "sbp!", 0

  tran1632 "lw", "lwp!", 2
  tran1632 "lh", "lhp!", 1
  tran1632 "lbu", "lbup!", 0
  tran1632 "sw", "swp!", 2
  tran1632 "sh", "shp!", 1
  tran1632 "sb", "sbp!", 0

