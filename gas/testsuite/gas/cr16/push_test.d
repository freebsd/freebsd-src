#as:
#objdump:  -dr
#name:  push_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	87 01       	push	\$0x1,r7,RA
   2:	96 01       	push	\$0x2,r6,RA
   4:	a5 01       	push	\$0x3,r5,RA
   6:	b4 01       	push	\$0x4,r4,RA
   8:	c3 01       	push	\$0x5,r3,RA
   a:	d2 01       	push	\$0x6,r2,RA
   c:	e1 01       	push	\$0x7,r1,RA
   e:	07 01       	push	\$0x1,r7
  10:	16 01       	push	\$0x2,r6
  12:	25 01       	push	\$0x3,r5
  14:	34 01       	push	\$0x4,r4
  16:	43 01       	push	\$0x5,r3
  18:	52 01       	push	\$0x6,r2
  1a:	61 01       	push	\$0x7,r1
  1c:	5c 01       	push	\$0x6,r12
  1e:	1e 01       	push	RA
  20:	1e 01       	push	RA
