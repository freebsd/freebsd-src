#objdump: -dwMsuffix
#name: x86-64 control register related opcodes (with suffixes)
#source: x86-64-crx.s

.*: +file format .*

Disassembly of section .text:

0+ <_start>:
[ 	]*[0-9a-f]+:	44 0f 20 c0[ 	]+movq[ 	]+?%cr8,%rax
[ 	]*[0-9a-f]+:	44 0f 20 c7[ 	]+movq[ 	]+?%cr8,%rdi
[ 	]*[0-9a-f]+:	44 0f 22 c0[ 	]+movq[ 	]+?%rax,%cr8
[ 	]*[0-9a-f]+:	44 0f 22 c7[ 	]+movq[ 	]+?%rdi,%cr8
[ 	]*[0-9a-f]+:	44 0f 20 c0[ 	]+movq[ 	]+?%cr8,%rax
[ 	]*[0-9a-f]+:	44 0f 20 c7[ 	]+movq[ 	]+?%cr8,%rdi
[ 	]*[0-9a-f]+:	44 0f 22 c0[ 	]+movq[ 	]+?%rax,%cr8
[ 	]*[0-9a-f]+:	44 0f 22 c7[ 	]+movq[ 	]+?%rdi,%cr8
[ 	]*[0-9a-f]+:	44 0f 20 c0[ 	]+movq[ 	]+?%cr8,%rax
[ 	]*[0-9a-f]+:	44 0f 20 c7[ 	]+movq[ 	]+?%cr8,%rdi
[ 	]*[0-9a-f]+:	44 0f 22 c0[ 	]+movq[ 	]+?%rax,%cr8
[ 	]*[0-9a-f]+:	44 0f 22 c7[ 	]+movq[ 	]+?%rdi,%cr8
