
.*:     file format elf32-xc16x

Disassembly of section .text:

00c00300 <_start>:
  c00300:	e0 f5       	mov r5,#0xf
  c00302:	e0 f6       	mov r6,#0xf
  c00304:	e0 f7       	mov r7,#0xf
  c00306:	e0 f8       	mov r8,#0xf
  c00308:	e0 f9       	mov r9,#0xf
  c0030a:	e0 fa       	mov r10,#0xf
  c0030c:	e0 fb       	mov r11,#0xf
  c0030e:	e0 fc       	mov r12,#0xf

00c00310 <.12>:
  c00310:	2d 07       	jmpr cc_Z,7
  c00312:	3d fe       	jmpr cc_NZ,254
  c00314:	8d fd       	jmpr cc_ULT,253
  c00316:	8d 45       	jmpr cc_ULT,69
  c00318:	9d 06       	jmpr cc_UGE,6
  c0031a:	0d 05       	jmpr cc_UC,5
  c0031c:	2d 04       	jmpr cc_Z,4
  c0031e:	3d 03       	jmpr cc_NZ,3

00c00320 <.13>:
  c00320:	fd 02       	jmpr cc_ULE,2
  c00322:	dd 01       	jmpr cc_SGE,1
  c00324:	bd 00       	jmpr cc_SLE,0

00c00326 <.end>:
  c00326:	1d f4       	jmpr cc_NET,244
  c00328:	bb fe       	callr 254
  c0032a:	bb fd       	callr 253
