/*
 * test relax
 * not.c <-> not! : register number must be in 0-15
 * neg.c <-> neg! : register number must be in 0-15
 * cmp.c <-> cmp! : register number must be in 0-15

 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16
.align 4

  \insn32 r0, r7          #32b -> 16b
  \insn16 r0, r7

  \insn32 r15, r4         #32b -> 16b
  \insn16 r15, r4

  \insn32 r15, r15        #32b -> 16b
  \insn16 r15, r15

  \insn16 r15, r3
  \insn32 r15, r3         #32b -> 16b

  \insn32 r8,  r2         #32b -> 16b
  \insn32 r8,  r2         #32b -> 16b
	
  \insn32 r15, r5         #No transform
  \insn32 r26, r23

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4
	
  \insn16 r0, r2         #16b -> 32b
  \insn32 r20, r21
	
  \insn16 r15, r4        #16b -> 32b
  \insn32 r25, r21
	
  \insn16 r15, r3        #16b -> 32b
  \insn32 r25, r22

  \insn16 r8, r3         #No transform
  \insn16 r8, r3         #No transform
	
  \insn16 r6, r2         #No transform
  \insn32 r6, r2         #32b -> 16b
	
  \insn32 r7, r4         #32b -> 16b
  \insn16 r7, r4         #No transform
	
.endm
		
.text
	
  tran3216 "not.c", "not!"
  tran3216 "neg.c", "neg!"
  tran3216 "cmp.c", "cmp!"

  tran1632 "not.c", "not!"
  tran1632 "neg.c", "neg!"
  tran1632 "cmp.c", "cmp!"
