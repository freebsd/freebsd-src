
.*:     file format elf32-xc16x

Disassembly of section .text:

00000400 <_start>:
 400:	e0 f8       	mov r8,#0xf
 402:	fa 00 08 04 	jmps #seg:0x0,#sof:0x408
 406:	e0 f9       	mov r9,#0xf

00000408 <.12>:
 408:	e0 f5       	mov r5,#0xf
 40a:	e0 f7       	mov r7,#0xf
 40c:	da 00 10 04 	calls #seg:0x0,#sof:0x410

00000410 <.13>:
 410:	e0 f6       	mov r6,#0xf
 412:	e0 f8       	mov r8,#0xf
