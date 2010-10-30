#as:
#objdump: -d
#source: bittst.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	6016      	bittst!		r0, 0x2
   2:	6016      	bittst!		r0, 0x2
   4:	6f26      	bittst!		r15, 0x4
   6:	6f26      	bittst!		r15, 0x4
   8:	6f0e      	bittst!		r15, 0x1
   a:	6f0e      	bittst!		r15, 0x1
   c:	6f1e      	bittst!		r15, 0x3
   e:	6f1e      	bittst!		r15, 0x3
  10:	6816      	bittst!		r8, 0x2
  12:	6816      	bittst!		r8, 0x2
  14:	800f842d 	bittst.c	r15, 0x1
  18:	801a902d 	bittst.c	r26, 0x4
  1c:	0000      	nop!
  1e:	0000      	nop!
  20:	8000882d 	bittst.c	r0, 0x2
  24:	8014882d 	bittst.c	r20, 0x2
  28:	81ef902d 	bittst.c	r15, 0x4
  2c:	8019902d 	bittst.c	r25, 0x4
  30:	81ef842d 	bittst.c	r15, 0x1
  34:	8019842d 	bittst.c	r25, 0x1
  38:	680e      	bittst!		r8, 0x1
  3a:	680e      	bittst!		r8, 0x1
  3c:	6626      	bittst!		r6, 0x4
  3e:	6626      	bittst!		r6, 0x4
  40:	671e      	bittst!		r7, 0x3
  42:	671e      	bittst!		r7, 0x3
#pass
