#as:
#objdump: -d
#source: presw.s

.*: +file format .*

Disassembly of section \.text:

00000000 <.text>:
   0:	202e      	push!		r0, \[r2\]
   2:	202e      	push!		r0, \[r2\]
   4:	27fe      	push!		r23, \[r7\]
   6:	27fe      	push!		r23, \[r7\]
   8:	2f0e      	push!		r15, \[r0\]
   a:	2f0e      	push!		r15, \[r0\]
   c:	2f7e      	push!		r15, \[r7\]
   e:	2f7e      	push!		r15, \[r7\]
  10:	29be      	push!		r25, \[r3\]
  12:	29be      	push!		r25, \[r3\]
  14:	8f0dffe4 	sw		r24, \[r13, -4\]\+
  18:	8ee7ffdc 	sw		r23, \[r7, -5\]\+
  1c:	0000      	nop!
  1e:	0000      	nop!
  20:	8c07ffe4 	sw		r0, \[r7, -4\]\+
  24:	8f2dffe4 	sw		r25, \[r13, -4\]\+
  28:	8f20ffe4 	sw		r25, \[r0, -4\]\+
  2c:	8e57ffe4 	sw		r18, \[r23, -4\]\+
  30:	263e      	push!		r6, \[r3\]
  32:	263e      	push!		r6, \[r3\]
  34:	237e      	push!		r3, \[r7\]
  36:	237e      	push!		r3, \[r7\]
#pass
