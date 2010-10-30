#as:
#objdump:  -dr
#name:  jal_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	d1 00       	jal	\(r2,r1\)
   2:	14 00 15 80 	jal	\(r6,r5\),\(r2,r1\)
   6:	14 00 32 80 	jal	\(r3,r2\),\(r4,r3\)
   a:	14 00 30 80 	jal	\(r1,r0\),\(r4,r3\)
   e:	14 00 72 80 	jal	\(r3,r2\),\(r8,r7\)
