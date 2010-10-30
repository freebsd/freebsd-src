
.*:     file format elf32-.*arm
architecture: arm, flags 0x00000150:
HAS_SYMS, DYNAMIC, D_PAGED
start address 0x.*

Disassembly of section .text:

.* <foo>:
 .*:	e1a00000 	nop			\(mov r0,r0\)
 .*:	e1a00000 	nop			\(mov r0,r0\)
 .*:	e1a0f00e 	mov	pc, lr
 .*:	00008098 	.word	0x00008098
 .*:	0000808c 	.word	0x0000808c
 .*:	00000004 	.word	0x00000004
