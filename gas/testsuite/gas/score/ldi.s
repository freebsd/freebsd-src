/*
 * test relax
 * ldi <-> ldiu! : for ldiu! : register number must be in 0-15, simm16:	[0-255]
 *   (1)ldi rD, simm16 : rD = simm16
 *   (2)ldiu! rD, imm8 : rD = ZE(imm8)
 	
 * Author: ligang
 */

/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16
.align 4
	
  \insn32 r2, 0                  #32b -> 16b
  \insn16 r2, 0

  \insn32 r3, 255                #32b -> 16b
  \insn16 r3, 255

  \insn32 r4, 9                  #32b -> 16b
  \insn32 r4, 9                  #32b -> 16b

  \insn16 r3, 255
  \insn32 r3, 255                #32b -> 16b
	
  \insn32 r8, 3                  #No transform
  \insn32 r25, 3                 #No transform

	
.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4
	
  \insn16 r2, 0                  #16b -> 32b
  \insn32 r25, 0  

  \insn16 r3, 255                #16b -> 32b
  \insn32 r23, 1  

  \insn16 r15, 255               #No transform
  \insn32 r15, 255

  \insn16 r8, 3                  #No transform
  \insn16 r8, 3                  #No transform

.endm

.text

  tran3216 "ldi", "ldiu!"
  tran1632 "ldi", "ldiu!"
