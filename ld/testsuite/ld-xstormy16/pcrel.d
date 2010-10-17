
.*:     file format elf32-xstormy16

Disassembly of section .text:

00008000 <_start>:
    8000:	00 79 46 80 	mov.w 0x0,#0x8046
    8004:	00 79 42 80 	mov.w 0x0,#0x8042
    8008:	00 79 44 80 	mov.w 0x0,#0x8044
    800c:	00 79 2c 00 	mov.w 0x0,#0x2c
    8010:	00 79 32 00 	mov.w 0x0,#0x32
    8014:	00 79 30 00 	mov.w 0x0,#0x30
    8018:	2c d3       	bc 0x8046
    801a:	26 d3       	bc 0x8042
    801c:	26 d3       	bc 0x8044
    801e:	24 c3 00 00 	bc Rx,#0x0,0x8046
    8022:	1c c3 00 00 	bc Rx,#0x0,0x8042
    8026:	1a c3 00 00 	bc Rx,#0x0,0x8044
    802a:	00 20 18 30 	bc r0,#0x0,0x8046
    802e:	00 20 10 30 	bc r0,#0x0,0x8042
    8032:	00 20 0e 30 	bc r0,#0x0,0x8044
    8036:	10 0d 0c 30 	bc r0,r1,0x8046
    803a:	10 0d 04 30 	bc r0,r1,0x8042
    803e:	10 0d 02 30 	bc r0,r1,0x8044

00008042 <global>:
    8042:	00 00       	nop

00008044 <local>:
    8044:	00 00       	nop

00008046 <external>:
    8046:	00 00       	nop
