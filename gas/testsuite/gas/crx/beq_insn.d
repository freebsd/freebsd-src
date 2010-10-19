#as:
#objdump: -dr
#name: beq_insn

.*: +file format .*

Disassembly of section .text:

00000000 <beq0b>:
   0:	aa b0       	beq0b	r10, 0x16 [-_<>+0-9a-z]*

00000002 <bne0b>:
   2:	fb b1       	bne0b	r11, 0x20 [-_<>+0-9a-z]*

00000004 <beq0w>:
   4:	0c b2       	beq0w	r12, 0x2 [-_<>+0-9a-z]*

00000006 <bne0w>:
   6:	fd b3       	bne0w	r13, 0x20 [-_<>+0-9a-z]*

00000008 <beq0d>:
   8:	fe b4       	beq0d	r14, 0x20 [-_<>+0-9a-z]*

0000000a <bne0d>:
   a:	7f b5       	bne0d	r15, 0x10 [-_<>+0-9a-z]*
