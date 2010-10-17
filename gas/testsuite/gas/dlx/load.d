#as:
#objdump: -dr
#name: load

.*: +file format .*

Disassembly of section .text:

00000000 <L-0x10>:
   0:	80 03 00 00 	lb      r3,0x0000\[r0\]
			2: R_DLX_RELOC_16_HI	.text
   4:	80 43 00 00 	lb      r3,0x0000\[r2\]
   8:	80 03 00 00 	lb      r3,0x0000\[r0\]
			a: R_DLX_RELOC_16_HI	.text
   c:	90 43 00 00 	lbu     r3,0x0000\[r2\]

00000010 <L>:
  10:	84 a3 00 00 	lh      r3,0x0000\[r5\]
			12: R_DLX_RELOC_16_HI	.text
  14:	95 e3 00 08 	lhu     r3,0x0008\[r15\]
  18:	8c 41 7f ff 	lw      r1,0x7fff\[r2\]
  1c:	8c 01 00 08 	lw      r1,0x0008\[r0\]
			1e: R_DLX_RELOC_16	.text
  20:	00 00 10 00 	nop
  24:	00 00 20 00 	nop
  28:	74 68 69 73 	sgei    r8,r3,0x6973
  2c:	20 69 73 20 	addi    r9,r3,0x7320
  30:	61 20 74 65 	seqi    r0,r9,0x7465
  34:	73 74 00 00 	slei    r20,r27,0x0000
	...
  40:	98 43 00 00 	ldstbu  r3,0x0000\[r2\]
  44:	9c 43 00 00 	ldsthu  r3,0x0000\[r2\]
  48:	b0 43 00 00 	ldstw   r3,0x0000\[r2\]
