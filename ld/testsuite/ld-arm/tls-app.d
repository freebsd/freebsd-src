
.*:     file format elf32-.*arm
architecture: arm, flags 0x00000112:
EXEC_P, HAS_SYMS, D_PAGED
start address 0x00008204

Disassembly of section .text:

00008204 <foo>:
    8204:	e1a00000 	nop			\(mov r0,r0\)
    8208:	e1a00000 	nop			\(mov r0,r0\)
    820c:	e1a0f00e 	mov	pc, lr
    8210:	000080bc 	.word	0x000080bc
    8214:	000080b4 	.word	0x000080b4
    8218:	000080ac 	.word	0x000080ac
    821c:	00000004 	.word	0x00000004
    8220:	000080c4 	.word	0x000080c4
    8224:	00000014 	.word	0x00000014
