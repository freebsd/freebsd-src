#objdump: -dw
#name: i386 cr8+

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
[ 	]*[0-9a-f]+:	f0 0f 20 c0[ 	]+movl?[ 	]+?%cr8,%eax
[ 	]*[0-9a-f]+:	f0 0f 20 c7[ 	]+movl?[ 	]+?%cr8,%edi
[ 	]*[0-9a-f]+:	f0 0f 22 c0[ 	]+movl?[ 	]+?%eax,%cr8
[ 	]*[0-9a-f]+:	f0 0f 22 c7[ 	]+movl?[ 	]+?%edi,%cr8
[ 	]*[0-9a-f]+:	f0 0f 20 c0[ 	]+movl?[ 	]+?%cr8,%eax
[ 	]*[0-9a-f]+:	f0 0f 20 c7[ 	]+movl?[ 	]+?%cr8,%edi
[ 	]*[0-9a-f]+:	f0 0f 22 c0[ 	]+movl?[ 	]+?%eax,%cr8
[ 	]*[0-9a-f]+:	f0 0f 22 c7[ 	]+movl?[ 	]+?%edi,%cr8
[ 	]*[0-9a-f]+:	f0 0f 20 c0[ 	]+movl?[ 	]+?%cr8,%eax
[ 	]*[0-9a-f]+:	f0 0f 20 c7[ 	]+movl?[ 	]+?%cr8,%edi
[ 	]*[0-9a-f]+:	f0 0f 22 c0[ 	]+movl?[ 	]+?%eax,%cr8
[ 	]*[0-9a-f]+:	f0 0f 22 c7[ 	]+movl?[ 	]+?%edi,%cr8
