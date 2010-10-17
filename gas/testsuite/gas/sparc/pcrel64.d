#as: -64 -K PIC
#objdump: -Dr
#name: pc relative 64-bit relocs

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <foo-0x8>:
   0:	01 00 00 00 	nop 
   4:	01 00 00 00 	nop 

0+8 <foo>:
   8:	01 00 00 00 	nop 
Disassembly of section .data:

0+ <.data>:
   0:	00 00 00 00 	illtrap  0
   4:	00 00 00 01 	illtrap  0x1
	...
			8: R_SPARC_32	.text\+0x10
			c: R_SPARC_DISP32	.text\+0x10
			10: R_SPARC_32	.text\+0x14
			14: R_SPARC_DISP32	.text\+0x14
			18: R_SPARC_32	foo
			1c: R_SPARC_DISP32	foo
			20: R_SPARC_32	foo\+0x10
			24: R_SPARC_DISP32	foo\+0x10
			28: R_SPARC_64	.text\+0x8
			30: R_SPARC_DISP64	.text\+0x8
			38: R_SPARC_64	foo
			40: R_SPARC_DISP64	foo
			48: R_SPARC_64	foo\+0x10
			50: R_SPARC_DISP64	foo\+0x10
			58: R_SPARC_DISP8	.data\+0x18
			59: R_SPARC_DISP8	.data\+0x64
			5a: R_SPARC_DISP16	.data\+0x18
			5c: R_SPARC_DISP16	.data\+0x64
  60:	00 02 00 00 	illtrap  0x20000
	...
