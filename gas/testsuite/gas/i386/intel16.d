#objdump: -dw -mi8086
#name: i386 intel16

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
   0:	66 0f bf 06 00 00 [ 	]*movswl 0,%eax
   6:	66 0f be 06 00 00 [ 	]*movsbl 0,%eax
   c:	0f be 06 00 00 [ 	]*movsbw 0,%ax
  11:	66 0f b7 06 00 00 [ 	]*movzwl 0,%eax
  17:	66 0f b6 06 00 00 [ 	]*movzbl 0,%eax
  1d:	0f b6 06 00 00 [ 	]*movzbw 0,%ax
	...
