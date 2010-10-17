#as:
#objdump: -dr
#name: itype

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	20 23 7f ff 	addi    r3,r1,0x7fff
   4:	24 23 ff fb 	addui   r3,r1,0xfffb
   8:	28 44 00 30 	subi    r4,r2,0x0030
   c:	2c 44 00 00 	subui   r4,r2,0x0000
			e: R_DLX_RELOC_16_HI	.text
  10:	30 c5 00 00 	andi    r5,r6,0x0000
			12: R_DLX_RELOC_16	.text
  14:	35 4c 00 78 	ori     r12,r10,0x0078
  18:	39 af 00 00 	xori    r15,r13,0x0000
			1a: R_DLX_RELOC_16_LO	.text
  1c:	da 30 00 1c 	slli    r16,r17,0x001c
			1e: R_DLX_RELOC_16	.text
  20:	e2 93 00 0f 	srai    r19,r20,0x000f
  24:	de f6 ff ff 	srli    r22,r23,0xffff
  28:	63 0f 7f ff 	seqi    r15,r24,0x7fff
  2c:	67 0f 7f ff 	snei    r15,r24,0x7fff
  30:	6b 0f 7f ff 	slti    r15,r24,0x7fff
  34:	6f 7a 00 00 	sgti    r26,r27,0x0000
  38:	73 bc a3 29 	slei    r28,r29,0xa329
  3c:	75 af 00 30 	sgei    r15,r13,0x0030
  40:	c3 0f 7f ff 	sequi   r15,r24,0x7fff
  44:	c7 0f 7f ff 	sneui   r15,r24,0x7fff
  48:	cb 0f 7f ff 	sltui   r15,r24,0x7fff
  4c:	cf 7a 00 00 	sgtui   r26,r27,0x0000
  50:	d3 bc ff fd 	sleui   r28,r29,0xfffd
  54:	d5 af 00 30 	sgeui   r15,r13,0x0030
  58:	20 01 80 03 	addi    r1,r0,0x8003
  5c:	21 28 00 00 	addi    r8,r9,0x0000
  60:	24 01 00 00 	addui   r1,r0,0x0000
			62: R_DLX_RELOC_16_HI	.text
  64:	25 28 00 00 	addui   r8,r9,0x0000
