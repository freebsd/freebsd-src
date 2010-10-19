
.*:     file format elf32-.*arm
architecture: arm, flags 0x00000150:
HAS_SYMS, DYNAMIC, D_PAGED
start address 0x.*

Disassembly of section .text:

.* <foo>:
 .*:	e1a00000 	nop			\(mov r0,r0\)
 .*:	e1a00000 	nop			\(mov r0,r0\)
 .*:	e1a0f00e 	mov	pc, lr
 .*:	00008098 	muleq	r0, r8, r0
 .*:	0000808c 	andeq	r8, r0, ip, lsl #1
 .*:	00000004 	andeq	r0, r0, r4
