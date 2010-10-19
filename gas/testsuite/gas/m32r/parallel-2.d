#as: -m32r2 -O
#objdump: -dr

.*: +file format .*

Disassembly of section .text:

0+0000 <test>:
   0:	04 a5 24 46 	add r4,r5 -> st r4,@r6
   4:	7c ff c6 04 	bc 0 <test> \|\| addi r6,[#]4
