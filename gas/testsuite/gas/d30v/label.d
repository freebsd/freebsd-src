#objdump: -dr
#name: D30V label alignment test
#as:

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <_abc-0x18>:
   0:	10080003 00f00000 	bra.s/tx	18	(18 <_abc>)	||	nop	
   8:	00f00000 00f00000 	nop		||	nop	
  10:	0e000004 00f00000 	.long	0xe000004	||	nop	

00000018 <_abc>:
  18:	00f00000 80f00000 	nop		->	nop	
  20:	00f00000 80f00000 	nop		->	nop	
