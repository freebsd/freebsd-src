#as: --m32rx
#objdump: -dr
#name: relax-2

.*: +file format .*

Disassembly of section .text:

0+0 <label1>:
   0:	fd 00 00 83 	bnc 20c <label3>
   4:	70 00 f0 00 	nop \|\| nop
   8:	43 03 c2 02 	addi r3,[#]3 \|\| addi r2,[#]2

0+0c <label2>:
	...

0+020c <label3>:
 20c:	70 00 f0 00 	nop \|\| nop
