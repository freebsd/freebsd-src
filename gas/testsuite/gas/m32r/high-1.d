#as:
#objdump: -dr
#name: high-1

.*: +file format .*

Disassembly of section .text:

0* <foo>:
 *0:	d4 c0 00 00 	seth r4,[#]*0x0
[ 	]*0: R_M32R_HI16_ULO_RELA	.text\+0x10000
 *4:	84 e4 00 00 	or3 r4,r4,[#]*0x0
[ 	]*4: R_M32R_LO16_RELA	.text\+0x10000
 *8:	d4 c0 12 34 	seth r4,[#]*0x1234
 *c:	84 e4 87 65 	or3 r4,r4,[#]*0x8765
 *10:	d4 c0 12 35 	seth r4,[#]*0x1235
 *14:	84 e4 87 65 	or3 r4,r4,[#]*0x8765
 *18:	d4 c0 87 65 	seth r4,[#]*0x8765
 *1c:	84 e4 43 21 	or3 r4,r4,[#]*0x4321
