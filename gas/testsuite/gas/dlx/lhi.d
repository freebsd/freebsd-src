#as:
#objdump: -dr
#name: lhi

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	3c 03 7f ff 	lhi     r3,0x7fff
   4:	3c 03 00 00 	lhi     r3,0x0000
			6: R_DLX_RELOC_16_HI	.text
   8:	3c 04 00 00 	lhi     r4,0x0000
			a: R_DLX_RELOC_16_LO	.text
   c:	3c 04 ff fb 	lhi     r4,0xfffb
			e: R_DLX_RELOC_16	.text
  10:	3c 04 00 0c 	lhi     r4,0x000c
  14:	20 04 00 00 	addi    r4,r0,0x0000
  18:	20 04 00 00 	addi    r4,r0,0x0000
			1a: R_DLX_RELOC_16_HI	.text
  1c:	34 84 00 18 	ori     r4,r4,0x0018
			1e: R_DLX_RELOC_16_LO	.text
  20:	20 64 00 00 	addi    r4,r3,0x0000
