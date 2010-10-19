#objdump: -dw -Msuffix
#name: i386 suffix

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	0f 01 c8 [ 	]*monitor %eax,%ecx,%edx
   3:	0f 01 c9 [ 	]*mwait  %eax,%ecx
   6:	0f 01 c1 [ 	]*vmcall 
   9:	0f 01 c2 [ 	]*vmlaunch 
   c:	0f 01 c3 [ 	]*vmresume 
   f:	0f 01 c4 [ 	]*vmxoff 
	...
