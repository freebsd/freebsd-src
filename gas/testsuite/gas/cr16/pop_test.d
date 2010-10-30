#as:
#objdump:  -dr
#name:  pop_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	87 02       	pop	\$0x1,r7,RA
   2:	96 02       	pop	\$0x2,r6,RA
   4:	a5 02       	pop	\$0x3,r5,RA
   6:	b4 02       	pop	\$0x4,r4,RA
   8:	c3 02       	pop	\$0x5,r3,RA
   a:	d2 02       	pop	\$0x6,r2,RA
   c:	e1 02       	pop	\$0x7,r1,RA
   e:	07 02       	pop	\$0x1,r7
  10:	16 02       	pop	\$0x2,r6
  12:	25 02       	pop	\$0x3,r5
  14:	34 02       	pop	\$0x4,r4
  16:	43 02       	pop	\$0x5,r3
  18:	52 02       	pop	\$0x6,r2
  1a:	61 02       	pop	\$0x7,r1
  1c:	1e 02       	pop	RA
