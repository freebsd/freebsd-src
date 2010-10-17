#objdump: -dr
#name: D30V alignment test
#as:

.*: +file format elf32-d30v

Disassembly of section .text:

0+0000 <start>:
   0:	08815a80 00f00000 	abs	r21, r42	||	nop	
   8:	08815a80 00f00000 	abs	r21, r42	||	nop	
  10:	08815a80 00f00000 	abs	r21, r42	||	nop	
  18:	00f00000 00f00000 	abs	r21, r42	||	nop	
  20:	08815a80 00f00000 	abs	r21, r42	||	nop	
  28:	08815a80 00f00000 	abs	r21, r42	||	nop	
  30:	08815a80 00f00000 	abs	r21, r42	||	nop	
	...
