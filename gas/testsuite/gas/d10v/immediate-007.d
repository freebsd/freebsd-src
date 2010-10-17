#objdump: -Dr
#source: immediate-007.s

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	ef 02 00 05 	repi	0x2, 14 <bar>
   4:	6f 00 5e 00 	nop		->	nop	
   8:	6f 00 5e 00 	nop		->	nop	
   c:	6f 00 5e 00 	nop		->	nop	
  10:	6f 00 5e 00 	nop		->	nop	
