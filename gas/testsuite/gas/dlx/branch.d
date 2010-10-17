#as:
#objdump: -dr
#name: branch

.*: +file format .*

Disassembly of section .text:

00000000 <L1>:
   0:	10 80 00 38 	beqz    r4,0x0000003c
   4:	00 00 00 00 	nop
   8:	14 a0 ff f4 	bnez    r5,0x00000000

0000000c <L2>:
   c:	20 04 00 44 	addi    r4,r0,0x0044
			e: R_DLX_RELOC_16	.text
  10:	08 00 00 30 	j       0x00000044
  14:	00 00 00 00 	nop
  18:	0c 00 00 20 	jal     0x0000003c
  1c:	00 00 00 00 	nop
  20:	50 00 00 18 	break   0x0000003c
  24:	00 00 00 00 	nop
  28:	47 ff ff d4 	trap    0x00000000
  2c:	00 00 00 00 	nop
  30:	4a 20 00 00 	jr      r17
  34:	00 00 00 00 	nop
  38:	4e 20 00 00 	jalr    r17

0000003c <L4>:
  3c:	8c 42 00 88 	lw      r2,0x0088\[r2\]
  40:	40 00 00 00 	rfe     0x00000044

00000044 <L5>:
  44:	8c 02 00 00 	lw      r2,0x0000\[r0\]
			46: R_DLX_RELOC_16	.text
  48:	0b ff ff b4 	j       0x00000000
  4c:	00 00 00 00 	nop
  50:	4b e0 00 00 	jr      r31
  54:	00 00 00 00 	nop
  58:	4b e0 00 00 	jr      r31
  5c:	00 00 00 00 	nop
  60:	48 20 00 00 	jr      r1
  64:	00 00 00 00 	nop
