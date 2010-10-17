#objdump: -dr
#name: D30V array test
#as:

.*: +file format elf32-d30v

Disassembly of section .text:

0+0000 <__foo-0x48>:
   0:	880820c0 80000048 	add.l	r2, r3, 0x48
			0: R_D30V_32	.text
   8:	880820c0 80000049 	add.l	r2, r3, 0x49
			8: R_D30V_32	.text
  10:	880820c0 8000004a 	add.l	r2, r3, 0x4a
			10: R_D30V_32	.text
  18:	880820c0 8000004b 	add.l	r2, r3, 0x4b
			18: R_D30V_32	.text
  20:	880820c0 8000004c 	add.l	r2, r3, 0x4c
			20: R_D30V_32	.text
  28:	880820c0 8000004d 	add.l	r2, r3, 0x4d
			28: R_D30V_32	.text
  30:	880820c0 8000004e 	add.l	r2, r3, 0x4e
			30: R_D30V_32	.text
  38:	880820c0 8000004f 	add.l	r2, r3, 0x4f
			38: R_D30V_32	.text
  40:	880820c0 80000050 	add.l	r2, r3, 0x50
			40: R_D30V_32	.text

0+0048 <__foo>:
  48:	12345678 12345678 	.long	0x12345678	||	.long	0x12345678
  50:	12345678 00000000      	.long	0x12345678	||	bra.s	r0
