#as:
#objdump: -d
#source: postlw.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	27fa      	pop!		r23, \[r7\]
   2:	27fa      	pop!		r23, \[r7\]
   4:	202a      	pop!		r0, \[r2\]
   6:	202a      	pop!		r0, \[r2\]
   8:	2f0a      	pop!		r15, \[r0\]
   a:	2f0a      	pop!		r15, \[r0\]
   c:	2f7a      	pop!		r15, \[r7\]
   e:	2f7a      	pop!		r15, \[r7\]
  10:	29ba      	pop!		r25, \[r3\]
  12:	29ba      	pop!		r25, \[r3\]
  14:	9f0d8020 	lw		r24, \[r13\]\+, 4
  18:	9ee78028 	lw		r23, \[r7\]\+, 5
  1c:	0000      	nop!
  1e:	0000      	nop!
  20:	9c078020 	lw		r0, \[r7\]\+, 4
  24:	9f2d8020 	lw		r25, \[r13\]\+, 4
  28:	9f208020 	lw		r25, \[r0\]\+, 4
  2c:	9e578020 	lw		r18, \[r23\]\+, 4
  30:	263a      	pop!		r6, \[r3\]
  32:	263a      	pop!		r6, \[r3\]
  34:	237a      	pop!		r3, \[r7\]
  36:	237a      	pop!		r3, \[r7\]
#pass
