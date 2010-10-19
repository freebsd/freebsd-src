#objdump: -dw -mi8086
#name: i386 intel16
#stderr: intel16.e

.*: +file format .*

Disassembly of section .text:

0+000 <.text>:
   0:	66 0f bf 06 00 00 [ 	]*movswl 0,%eax
   6:	66 0f be 06 00 00 [ 	]*movsbl 0,%eax
   c:	0f be 06 00 00 [ 	]*movsbw 0,%ax
  11:	66 0f b7 06 00 00 [ 	]*movzwl 0,%eax
  17:	66 0f b6 06 00 00 [ 	]*movzbl 0,%eax
  1d:	0f b6 06 00 00 [ 	]*movzbw 0,%ax
  22:	8d 00 [ 	]*lea    \(%bx,%si\),%ax
  24:	8d 02 [ 	]*lea    \(%bp,%si\),%ax
  26:	8d 01 [ 	]*lea    \(%bx,%di\),%ax
  28:	8d 03 [ 	]*lea    \(%bp,%di\),%ax
  2a:	8d 00 [ 	]*lea    \(%bx,%si\),%ax
  2c:	8d 02 [ 	]*lea    \(%bp,%si\),%ax
  2e:	8d 01 [ 	]*lea    \(%bx,%di\),%ax
  30:	8d 03 [ 	]*lea    \(%bp,%di\),%ax
	...
