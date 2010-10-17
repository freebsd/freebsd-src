#as: -K PIC -64
#objdump: -Dr
#name: plt 64-bit relocs

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <.text>:
   0:	40 00 00 00 	call  0x0
			0: R_SPARC_WPLT30	foo
   4:	01 00 00 00 	nop 
   8:	40 00 00 00 	call  0x8
			8: R_SPARC_WPLT30	bar\+0x4
Disassembly of section .data:

0+ <.data>:
	...
			0: R_SPARC_PLT64	foo
			8: R_SPARC_PLT64	bar\+0x4
  10:	01 00 00 00 	nop 
			11: R_SPARC_PLT64	foo
  14:	00 00 00 00 	illtrap  0
  18:	00 02 03 04 	illtrap  0x20304
  1c:	00 00 00 00 	illtrap  0
			1c: R_SPARC_PLT32	bar\+0x4
