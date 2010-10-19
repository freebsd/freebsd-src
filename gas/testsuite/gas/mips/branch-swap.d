#as: -march=mips2 -32
#objdump: -dr
#name: MIPS branch-swap

.*:     file format .*mips.*

Disassembly of section \.text:

00000000 <foo-0x10>:
   0:	5040ffff 	beqzl	v0,0 <foo-0x10>
   4:	00000000 	nop
   8:	1000fffd 	b	0 <foo-0x10>
   c:	00000000 	nop

00000010 <foo>:
  10:	5040ffff 	beqzl	v0,10 <foo>
  14:	00000000 	nop
  18:	1000fffd 	b	10 <foo>
  1c:	00000000 	nop
	\.\.\.
