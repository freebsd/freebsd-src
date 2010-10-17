#as:
#objdump: -dr
#name: lohi

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	00 00 00 00 	nop
   4:	3c 01 00 03 	lhi     r1,0x0003
			6: R_DLX_RELOC_16_HI	.text
   8:	34 01 0d 44 	ori     r1,r0,0x0d44
			a: R_DLX_RELOC_16_LO	.text
   c:	3c 01 0b eb 	lhi     r1,0x0beb
			e: R_DLX_RELOC_16_HI	.text
  10:	34 01 c2 04 	ori     r1,r0,0xc204
			12: R_DLX_RELOC_16_LO	.text
