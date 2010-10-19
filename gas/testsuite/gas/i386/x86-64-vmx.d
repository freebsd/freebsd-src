#objdump: -dw
#name: 64bit VMX

.*: +file format .*

Disassembly of section .text:

0+000 <foo>:
   0:	0f 01 c1 [ 	]*vmcall 
   3:	0f 01 c2 [ 	]*vmlaunch 
   6:	0f 01 c3 [ 	]*vmresume 
   9:	0f 01 c4 [ 	]*vmxoff 
   c:	66 0f c7 30 [ 	]*vmclear \(%rax\)
  10:	0f c7 30 [ 	]*vmptrld \(%rax\)
  13:	0f c7 38 [ 	]*vmptrst \(%rax\)
  16:	f3 0f c7 30 [ 	]*vmxon  \(%rax\)
  1a:	0f 78 c3 [ 	]*vmread %rax,%rbx
  1d:	0f 78 c3 [ 	]*vmread %rax,%rbx
  20:	0f 78 03 [ 	]*vmread %rax,\(%rbx\)
  23:	0f 78 03 [ 	]*vmread %rax,\(%rbx\)
  26:	0f 79 d8 [ 	]*vmwrite %rax,%rbx
  29:	0f 79 d8 [ 	]*vmwrite %rax,%rbx
  2c:	0f 79 18 [ 	]*vmwrite \(%rax\),%rbx
  2f:	0f 79 18 [ 	]*vmwrite \(%rax\),%rbx
	...
