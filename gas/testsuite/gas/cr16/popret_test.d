#as:
#objdump:  -dr
#name:  popret_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	87 03       	popret	\$0x1,r7,RA
   2:	96 03       	popret	\$0x2,r6,RA
   4:	a5 03       	popret	\$0x3,r5,RA
   6:	b4 03       	popret	\$0x4,r4,RA
   8:	c3 03       	popret	\$0x5,r3,RA
   a:	d2 03       	popret	\$0x6,r2,RA
   c:	e1 03       	popret	\$0x7,r1,RA
   e:	07 03       	popret	\$0x1,r7
  10:	16 03       	popret	\$0x2,r6
  12:	25 03       	popret	\$0x3,r5
  14:	34 03       	popret	\$0x4,r4
  16:	43 03       	popret	\$0x5,r3
  18:	52 03       	popret	\$0x6,r2
  1a:	61 03       	popret	\$0x7,r1
  1c:	1e 03       	popret	RA
