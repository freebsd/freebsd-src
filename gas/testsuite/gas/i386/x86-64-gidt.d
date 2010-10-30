#objdump: -dw
#name: 64bit load/store global/interrupt description table register.

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	0f 01 08 [ 	]*sidt   \(%rax\)
   3:	0f 01 18 [ 	]*lidt   \(%rax\)
   6:	0f 01 00 [ 	]*sgdt   \(%rax\)
   9:	0f 01 10 [ 	]*lgdt   \(%rax\)
   c:	0f 01 08 [ 	]*sidt   \(%rax\)
   f:	0f 01 18 [ 	]*lidt   \(%rax\)
  12:	0f 01 00 [ 	]*sgdt   \(%rax\)
  15:	0f 01 10 [ 	]*lgdt   \(%rax\)
	...
