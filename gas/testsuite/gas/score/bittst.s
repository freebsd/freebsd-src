/*
 * test relax
 * bittst.c <-> bittst! : register number must be in 0-15

 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16

  \insn32 r0, 2         #32b -> 16b
  \insn16 r0, 2

  \insn32 r15, 4        #32b -> 16b
  \insn16 r15, 4

  \insn32 r15, 1        #32b -> 16b
  \insn16 r15, 1

  \insn16 r15, 3
  \insn32 r15, 3        #32b -> 16b

  \insn32 r8,  2        #32b -> 16b
  \insn32 r8,  2        #32b -> 16b

  \insn32 r15, 1        #No transform
  \insn32 r26, 4

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4

  \insn16 r0, 2         #16b -> 32b
  \insn32 r20, 2         

  \insn16 r15, 4        #16b -> 32b
  \insn32 r25, 4

  \insn16 r15, 1        #16b -> 32b
  \insn32 r25, 1

  \insn16 r8, 1         #No transform
  \insn16 r8, 1         #No transform
	
  \insn16 r6, 4         #No transform
  \insn32 r6, 4         #32b -> 16b

  \insn32 r7, 3         #32b -> 16b
  \insn16 r7, 3         #No transform
	
.endm

.text

  tran3216 "bittst.c", "bittst!"
  tran1632 "bittst.c", "bittst!"
	
