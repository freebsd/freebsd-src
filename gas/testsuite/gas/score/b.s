/*
 * test relax
 * b <-> b! : jump range must be in 8 bit, only 32b -> 16b
	
 * Author: ligang
 */

.macro tran insn32, insn16
/* This block transform 32b instruction to 16b. */
.align 4

  \insn32               #32b -> 16b
  \insn16

  \insn32               #32b -> 16b
  \insn32               #32b -> 16b

  \insn16      
  \insn32               #32b -> 16b

  \insn32               #No transform
  add r18, r20, r24

.endm

L1:
	
  tran "b L1", "b! L1"
  #tran "b 0x8", "b! 0x8"
 
