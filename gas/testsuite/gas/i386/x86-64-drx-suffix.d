#objdump: -dwMsuffix
#name: x86-64 debug register related opcodes (with suffixes)
#source: x86-64-drx.s

.*: +file format elf64-x86-64

Disassembly of section .text:

0+ <_start>:
[ 	]*[0-9a-f]+:	44 0f 21 c0[ 	]+movq[ 	]+?%db8,%rax
[ 	]*[0-9a-f]+:	44 0f 21 c7[ 	]+movq[ 	]+?%db8,%rdi
[ 	]*[0-9a-f]+:	44 0f 23 c0[ 	]+movq[ 	]+?%rax,%db8
[ 	]*[0-9a-f]+:	44 0f 23 c7[ 	]+movq[ 	]+?%rdi,%db8
[ 	]*[0-9a-f]+:	44 0f 21 c0[ 	]+movq[ 	]+?%db8,%rax
[ 	]*[0-9a-f]+:	44 0f 21 c7[ 	]+movq[ 	]+?%db8,%rdi
[ 	]*[0-9a-f]+:	44 0f 23 c0[ 	]+movq[ 	]+?%rax,%db8
[ 	]*[0-9a-f]+:	44 0f 23 c7[ 	]+movq[ 	]+?%rdi,%db8
[ 	]*[0-9a-f]+:	44 0f 21 c0[ 	]+movq[ 	]+?%db8,%rax
[ 	]*[0-9a-f]+:	44 0f 21 c7[ 	]+movq[ 	]+?%db8,%rdi
[ 	]*[0-9a-f]+:	44 0f 23 c0[ 	]+movq[ 	]+?%rax,%db8
[ 	]*[0-9a-f]+:	44 0f 23 c7[ 	]+movq[ 	]+?%rdi,%db8
