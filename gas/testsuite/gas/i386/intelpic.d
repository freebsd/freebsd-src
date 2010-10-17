#as: -J
#objdump: -dw
#name: i386 intelpic

.*: +file format .*

Disassembly of section .text:

0+000 <gs_foo>:
   0:	c3 [ 	]*ret    

0+001 <bar>:
   1:	8d 83 00 00 00 00 [ 	]*lea    0x0\(%ebx\),%eax
   7:	8b 83 00 00 00 00 [ 	]*mov    0x0\(%ebx\),%eax
   d:	90 [ 	]*nop    
[ 	]*...
