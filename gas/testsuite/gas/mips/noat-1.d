#as: -32 -mips1
#objdump: -dr

.*: +file format .*mips

Disassembly of section \.text:

00000000 <\.text>:
   0:	8f7b7fff 	lw	k1,32767\(k1\)
   4:	00000000 	nop
   8:	8f7b8000 	lw	k1,-32768\(k1\)
   c:	00000000 	nop
  10:	af7b7fff 	sw	k1,32767\(k1\)
  14:	af7b8000 	sw	k1,-32768\(k1\)
	\.\.\.
