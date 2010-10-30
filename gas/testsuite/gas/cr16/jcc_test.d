#as:
#objdump:  -dr
#name:  jcc_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 0a       	jeq	\(r1,r0\)
   2:	11 0a       	jne	\(r2,r1\)
   4:	32 0a       	jcc	\(r3,r2\)
   6:	33 0a       	jcc	\(r4,r3\)
   8:	44 0a       	jhi	\(r5,r4\)
   a:	c5 0a       	jlt	\(r6,r5\)
   c:	66 0a       	jgt	\(r7,r6\)
   e:	87 0a       	jfs	\(r8,r7\)
  10:	98 0a       	jfc	\(r9,r8\)
  12:	a9 0a       	jlo	\(r10,r9\)
  14:	4a 0a       	jhi	\(r11,r10\)
  16:	c0 0a       	jlt	\(r1,r0\)
  18:	d2 0a       	jge	\(r3,r2\)
  1a:	e5 0a       	jump	\(r6,r5\)
  1c:	f5 0a       	jusr	\(r6,r5\)
