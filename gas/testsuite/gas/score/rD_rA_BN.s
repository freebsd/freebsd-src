/*
 * test relax
 * bitclr.c <-> bitclr! : register number must be in 0-15
 * bitset.c <-> bitset! : register number must be in 0-15
 * bittgl.c <-> bittgl! : register number must be in 0-15
 * slli.c <-> slli!     : register number must be in 0-15
 * srli.c <-> srli!     : register number must be in 0-15

 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16
.align 4

  \insn32 r0, r0, 2          #32b -> 16b
  \insn16 r0, 2

  \insn32 r15, r15, 4        #32b -> 16b
  \insn16 r15, 4

  \insn32 r15, r15, 1        #32b -> 16b
  \insn16 r15, 1

  \insn16 r15, 3
  \insn32 r15, r15, 3        #32b -> 16b

  \insn32 r8, r8, 3          #32b -> 16b
  \insn32 r8, r8, 3          #32b -> 16b
	
  \insn32 r15, r15, 1        #No transform
  \insn32 r26, r23, 4

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4

  \insn16 r0, 2         #16b -> 32b
  \insn32 r20, r21, 2
	
  \insn16 r15, 4        #16b -> 32b
  \insn32 r25, r21, 4

  \insn16 r15, 1        #16b -> 32b
  \insn32 r25, r22, 1

  \insn16 r8, 3         #No transform
  \insn16 r8, 3         #No transform
	
  \insn16 r6, 4         #No transform
  \insn32 r6, r6, 4     #32b -> 16b

  \insn32 r9, r9, 2     #32b -> 16b
  \insn16 r9, 2         #No transform  
	
.endm
		
.text

  tran3216 "bitclr.c", "bitclr!"
  tran3216 "bitset.c", "bitset!"
  tran3216 "bittgl.c", "bittgl!"
  tran3216 "slli.c", "slli!"
  tran3216 "srli.c", "srli!"

  tran1632 "bitclr.c", "bitclr!"
  tran1632 "bitset.c", "bitset!"
  tran1632 "bittgl.c", "bittgl!"
  tran1632 "slli.c", "slli!"
  tran1632 "srli.c", "srli!"
	
