#objdump: -dw
#name: i386 prefix

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	9b 67 26 d9 3c [ 	]*addr16 fstcw %es:\(%si\)
   5:	9b df e0 [ 	]*fstsw  %ax
   8:	9b df e0 [ 	]*fstsw  %ax
   b:	9b df e0 [ 	]*fstsw  %ax
   e:	9b 67 df e0 [ 	]*addr16 fstsw %ax
  12:	f3 67 66 36 a7 [ 	]*repz addr16 cmpsw %es:\(%di\),%ss:\(%si\)
#pass
