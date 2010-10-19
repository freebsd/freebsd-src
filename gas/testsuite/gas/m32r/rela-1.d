#as:
#objdump: -dr
#name: rela-1

.*: +file format .*

Disassembly of section .text:

0+0000 <.text>:
   0:	fe 00 00 00 	bl 0 <.text>
			0: R_M32R_26_PCREL_RELA	.text2\+0x8
   4:	fe 00 00 00 	bl 4 <.text\+0x4>
			4: R_M32R_26_PCREL_RELA	.text2\+0x8
   8:	7e 00 f0 00 	bl 8 <.text\+0x8> \|\| nop
			8: R_M32R_10_PCREL_RELA	.text2\+0x8
   c:	b0 90 00 00 	bnez r0,c <.text\+0xc>
			c: R_M32R_18_PCREL_RELA	.text2\+0x8
  10:	10 80 7e 00 	mv r0,r0 -> bl 10 <.text\+0x10>
			12: R_M32R_10_PCREL_RELA	.text2\+0x8
Disassembly of section .text2:

0+0000 <label-0x8>:
   0:	70 00 70 00 	nop -> nop
   4:	70 00 70 00 	nop -> nop
