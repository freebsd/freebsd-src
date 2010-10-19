
.*:     file format elf32-.*arm
architecture: arm, flags 0x00000112:
EXEC_P, HAS_SYMS, D_PAGED
start address 0x00008220

Disassembly of section .text:

00008220 <foo>:
    8220:	e1a00000 	nop			\(mov r0,r0\)
    8224:	e1a00000 	nop			\(mov r0,r0\)
    8228:	e1a0f00e 	mov	pc, lr
    822c:	000080bc 	streqh	r8, \[r0\], -ip
    8230:	000080b4 	streqh	r8, \[r0\], -r4
    8234:	000080ac 	andeq	r8, r0, ip, lsr #1
    8238:	00000004 	andeq	r0, r0, r4
    823c:	000080c4 	andeq	r8, r0, r4, asr #1
    8240:	00000014 	andeq	r0, r0, r4, lsl r0
