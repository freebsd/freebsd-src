/*
 * test relax
 * lw <-> lw!   : register number must be in 0-15, offset == 0
 * lh <-> lh!   : register number must be in 0-15, offset == 0
 * lbu <-> lbu! : register number must be in 0-15, offset == 0
 * sw <-> sw!   : register number must be in 0-15, offset == 0
 * sh <-> sh!   : register number must be in 0-15, offset == 0
 * sb <-> sb!   : register number must be in 0-15, offset == 0

 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16
.align 4

  \insn32 r0, [r3, 0]     #32b -> 16b
  \insn16 r0, [r3]

  \insn32 r3, [r15, 0]    #32b -> 16b
  \insn16 r3, [r15]

  \insn32 r15, [r8, 0]    #32b -> 16b
  \insn16 r15, [r8]

  \insn32 r4, [r8, 0]     #No transform
  \insn32 r25, [r19, 0]

  \insn32 r5, [r7, 0]     #32b -> 16b
  \insn32 r5, [r7, 0]     #32b -> 16b

  \insn16 r2, [r3]  
  \insn32 r2, [r3, 0]     #32b -> 16b

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4

  \insn16 r0, [r3]        #16b -> 32b
  \insn32 r18, [r23, 10]     

  \insn16 r15, [r0]       #16b -> 32b
  \insn32 r17, [r26, 10]     

  \insn16 r6, [r8]        #No transform
  \insn16 r6, [r8]        #No transform

  \insn16 r3, [r7]        #No transform
  \insn32 r3, [r7, 0]

.endm
.space 1
  tran3216 "lw", "lw!"
.fill 10, 1
  tran3216 "lh", "lh!"
.org 0x101
  tran3216 "lbu", "lbu!"
.org 0x203
  tran3216 "sw", "sw!"
  tran3216 "sh", "sh!"
  tran3216 "sb", "sb!"

  tran1632 "lw", "lw!"
  tran1632 "lh", "lh!"
  tran1632 "lbu", "lbu!"
  tran1632 "sw", "sw!"
  tran1632 "sh", "sh!"
  tran1632 "sb", "sb!"
