
.*:     file format elf32-xc16x

Disassembly of section .text:

00000400 <_start>:
 400:	e0 f5       	mov r5,#0xf
 402:	e0 f6       	mov r6,#0xf
 404:	e0 f7       	mov r7,#0xf
 406:	e0 f8       	mov r8,#0xf
 408:	e0 f9       	mov r9,#0xf
 40a:	e0 fa       	mov r10,#0xf
 40c:	e0 fb       	mov r11,#0xf
 40e:	e0 fc       	mov r12,#0xf

00000410 <.12>:
 410:	2d 07       	jmpr cc_Z,7
 412:	3d fe       	jmpr cc_NZ,254
 414:	8d fd       	jmpr cc_ULT,253
 416:	8d 45       	jmpr cc_ULT,69
 418:	9d 06       	jmpr cc_UGE,6
 41a:	0d 05       	jmpr cc_UC,5
 41c:	2d 04       	jmpr cc_Z,4
 41e:	3d 03       	jmpr cc_NZ,3

00000420 <.13>:
 420:	fd 02       	jmpr cc_ULE,2
 422:	dd 01       	jmpr cc_SGE,1
 424:	bd 00       	jmpr cc_SLE,0

00000426 <.end>:
 426:	1d f4       	jmpr cc_NET,244
 428:	bb fe       	callr 254
 42a:	bb fd       	callr 253
