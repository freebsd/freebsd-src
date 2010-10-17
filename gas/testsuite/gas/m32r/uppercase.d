#as:
#objdump: -dr
#name: uppercase

.*: +file format .*

Disassembly of section .text:

0+0000 <foo>:
   0:	10 81 10 91 *	mv r0,r1 -> mvfc r0,cbr

0+0004 <high>:
   4:	d0 c0 00 00 	seth r0,#0x0
[ 	]*4: R_M32R_HI16_ULO_RELA	[.]text\+0x4

0+0008 <shigh>:
   8:	d0 c0 00 00 	seth r0,#0x0
[ 	]*8: R_M32R_HI16_SLO_RELA	[.]text\+0x8

0+000c <low>:
   c:	80 e0 00 00 	or3 r0,r0,#0x0
[ 	]*c: R_M32R_LO16_RELA	[.]text\+0xc

0+0010 <sda>:
  10:	80 a0 00 00 	add3 r0,r0,#0
[ 	]*10: R_M32R_SDA16_RELA	sdavar
