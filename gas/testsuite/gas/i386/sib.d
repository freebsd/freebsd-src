#objdump: -dw
#name: i386 SIB

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	8b 04 23 [ 	]*mov [ 	]*\(%ebx\),%eax
   3:	8b 04 63 [ 	]*mov [ 	]*\(%ebx\),%eax
   6:	8b 04 a3 [ 	]*mov [ 	]*\(%ebx\),%eax
   9:	8b 04 e3 [ 	]*mov [ 	]*\(%ebx\),%eax
   c:	90 [ 	]*nop [ 	]*
   d:	90 [ 	]*nop [ 	]*
	...
