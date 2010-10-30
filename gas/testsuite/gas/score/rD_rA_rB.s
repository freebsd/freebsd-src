/*
 * test relax
 * add.c  <-> add!  : register number must be in 0-15
 * addc.c <-> addc! : register number must be in 0-15
 * sub.c  <-> sub!  : register number must be in 0-15
 * and.c  <-> and!  : register number must be in 0-15
 * or.c   <-> or!   : register number must be in 0-15
 * xor.c  <-> xor!  : register number must be in 0-15
 * sra.c  <-> sra!  : register number must be in 0-15
 * srl.c  <-> srl!  : register number must be in 0-15
 * sll.c  <-> sll!  : register number must be in 0-15

 * Author: ligang
 */


/* This macro transform 32b instruction to 16b. */
.macro tran3216 insn32, insn16
.align 4

  \insn32 r0, r0, r2          #32b -> 16b
  \insn16 r0, r2

  \insn32 r5, r5, r4          #32b -> 16b
  \insn16 r5, r4

  \insn32 r15, r15, r4        #32b -> 16b
  \insn16 r15, r4

  \insn16 r15, r3
  \insn32 r15, r15, r3        #32b -> 16b

  \insn32 r8, r8, r3          #32b -> 16b
  \insn32 r8, r8, r3          #32b -> 16b
	
  \insn32 r15, r15, r6        #No transform
  \insn32 r26, r23, r4

.endm

/* This macro transform 16b instruction to 32b. */
.macro tran1632 insn32, insn16
.align 4

  \insn16 r0, r2         #16b -> 32b
  \insn32 r20, r21, r2

  \insn16 r15, r4        #16b -> 32b
  \insn32 r25, r21, r4
	
  \insn16 r15, r3        #16b -> 32b
  \insn32 r25, r22, r3

  \insn16 r8, r7         #No transform
  \insn16 r8, r7         #No transform
		
  \insn16 r6, r4         #No transform
  \insn32 r6, r6, r4
	
  \insn32 r7, r7, r4     #32b -> 16b
  \insn16 r7, r4         #No transform
	
.endm
		
.text
	
  tran3216 "add.c", "add!"
  tran3216 "addc.c", "addc!"
  tran3216 "sub.c", "sub!"
  tran3216 "and.c", "and!"
  tran3216 "or.c", "or!"
  tran3216 "xor.c", "xor!"
  tran3216 "sra.c", "sra!"
  tran3216 "srl.c", "srl!"
  tran3216 "sll.c", "sll!"	

  tran1632 "add.c", "add!"
  tran1632 "addc.c", "addc!"
  tran1632 "sub.c", "sub!"
  tran1632 "and.c", "and!"
  tran1632 "or.c", "or!"
  tran1632 "xor.c", "xor!"
  tran1632 "sra.c", "sra!"
  tran1632 "srl.c", "srl!"
  tran1632 "sll.c", "sll!"	

