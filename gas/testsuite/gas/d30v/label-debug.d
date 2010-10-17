#objdump: -ldr
#name: D30V debug (-g) test
#as: -g

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <_abc-0x18>:
.*label-debug.s:4
   0:	10080003 00f00000 	bra.s\/tx	18	\(18 <_abc>\)	\|\|	nop	
.*label-debug.s:5
   8:	00f00000 00f00000 	nop		||	nop	
  10:	0e000004 00f00000 	.long	0xe000004	||	nop	

00000018 <_abc>:
.*label-debug.s:8
  18:	00f00000 00f00000 	nop		||	nop	
.*label-debug.s:9
  20:	00f00000 00f00000 	nop		||	nop	
.*label-debug.s:10
  28:	00f00000 00f00000 	nop		||	nop	
.*label-debug.s:11
  30:	00f00000 00f00000 	nop		||	nop	
