/*
 * test relax
 * br <-> br!   : register number must be in 0-15
 * brl <-> brl! : register number must be in 0-15

 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16
.align 4

  \insn32 r0      #32b -> 16b
  \insn16 r0

  \insn32 r15     #32b -> 16b
  \insn16 r15

  \insn32 r3      #32b -> 16b
  \insn32 r3      #32b -> 16b

  \insn16 r5      
  \insn32 r5      #32b -> 16b

  \insn32 r3      #No transform
  \insn32 r31     #No transform

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4

  \insn16 r0        #16b -> 32b
  \insn32 r23    

  \insn16 r15       #16b -> 32b
  \insn32 r27     

  \insn16 r6        #No transform
  \insn32 r6

  \insn16 r3        #No transform
  \insn16 r3

.endm

  tran3216 "br", "br!"
  tran3216 "brl", "brl!"

  tran1632 "br", "br!"
  tran1632 "brl", "brl!"

