/*
 * test relax
 * addi <-> addei! : for addei : register number must be in 0-15, offset : 4b, only 16b -> 32b
 *   (1)addi rD, simm16 : rD = rD + simm16, -32768 <= simm16 <= 32767
 *   (2)addei! rD, imm4 : rD = rD + 2**imm4
 * addi <-> subei! : for addei : register number must be in 0-15, offset : 4b, only 16b -> 32b
 *   (1)addi rD, simm16 : rD = rD + simm16, -32768 <= simm16 <= 32767
 *   (2)subei! rD, imm4 : rD = rD + 2**imm4
	
 * Author: ligang
 */

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16, sign
.align 4
	
  \insn16 r0, 0                  #16b -> 32b
  \insn32 r0, \sign * 1         

  \insn16 r15, 4                 #16b -> 32b
  \insn32 r15, \sign * 16

  \insn16 r15, 14                #16b -> 32b
  \insn32 r15, \sign * 1024 * 16

  \insn16 r8, 3                  #No transform
  \insn16 r8, 3                  #No transform

  \insn16 r15, 15                #No transform. Because 2**15 = 32768, extend range of addi
  \insn32 r15, 0x7FFF

.endm

.text

  tran1632 "addi.c", "addei!", 1
  tran1632 "addi.c", "subei!", -1
