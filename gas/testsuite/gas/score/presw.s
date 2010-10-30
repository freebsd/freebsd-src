/*
 * test relax
 * pre sw <-> push! : offset == -4
 * syntax:	
   sw rD, [rA, simm12]+ : rD and rA can be 0-31
   push! rD, [rAg0] : rAg0 must be in 0-7, rD can be 0-31

 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16
.align 4

  \insn32 r0, [r2, -4]+     #32b -> 16b
  \insn16 r0, [r2]

  \insn32 r23, [r7, -4]+    #32b -> 16b
  \insn16 r23, [r7]

  \insn32 r15, [r0, -4]+    #32b -> 16b
  \insn16 r15, [r0]

  \insn16 r15, [r7]
  \insn32 r15, [r7, -4]+    #32b -> 16b
	
  \insn32 r25, [r3, -4]+    #32b -> 16b
  \insn32 r25, [r3, -4]+    #32b -> 16b

  \insn32 r24, [r13, -4]+   #No transform
  \insn32 r23, [r7, -5]+    #No transform

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4

  \insn16 r0, [r7]         #16b -> 32b
  \insn32 r25, [r13, -4]+

  \insn16 r25, [r0]        #16b -> 32b
  \insn32 r18, [r23, -4]+

  \insn16 r6, [r3]         #No transform
  \insn16 r6, [r3]         #No transform

  \insn16 r3, [r7]         #No transform
  \insn32 r3, [r7, -4]+

.endm

  tran3216 "sw", "push!"
  tran1632 "sw", "push!"
