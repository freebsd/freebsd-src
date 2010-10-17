#as: -EL
#objdump: -dr -EL

.*: +file format elf32-.*arc

Disassembly of section .text:

00000000 <.text>:
   0:	00 80 00 08 	08008000     ld         r0,\[r1\]
   4:	01 00 a3 08 	08a30001     ld         r5,\[r6,1\]
   8:	00 00 7f 0a 	0a7f0000     ld         r19,\[0\]
   c:	00 00 00 00 
			c: R_ARC_32	foo
  10:	0a 10 81 08 	0881100a     ld.a       r4,\[r2,10\]
  14:	00 00 3f 08 	083f0000     ld         r1,\[0x384\]
  18:	84 03 00 00 
  1c:	0f 84 41 08 	0841840f     ldb        r2,\[r3,15\]
  20:	fe 09 62 08 	086209fe     ldw        r3,\[r4,-2\]
  24:	00 20 21 08 	08212000     lr         r1,\[r2\]
  28:	14 a0 3f 08 	083fa014     lr         r1,\[0x14\]
  2c:	00 a0 1f 08 	081fa000     lr         r0,\[status\]
