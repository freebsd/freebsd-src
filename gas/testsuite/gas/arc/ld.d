#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 84 00 00 	00008400     ld         r0,\[r1,r2\]
   4:	02 84 00 00 	00008402     ldb        r0,\[r1,r2\]
   8:	08 88 21 00 	00218808     ld.a       r1,\[r3,r4\]
   c:	05 06 21 00 	00210605     ldw.x      r1,\[r2,r3\]
  10:	0d 88 41 00 	0041880d     ldw.x.a    r2,\[r3,r4\]
  14:	00 80 1f 08 	081f8000     ld         r0,\[0\]
  18:	1e 80 00 08 	0800801e     ld         r0,\[r1,30\]
  1c:	ec 01 21 08 	082101ec     ld         r1,\[r2,-20\]
