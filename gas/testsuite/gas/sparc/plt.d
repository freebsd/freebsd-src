#as: -K PIC
#objdump: -Dr
#name: plt relocs

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
			0: R_SPARC_PLT32	foo
			4: R_SPARC_PLT32	bar\+0x4
   8:	01 00 00 00 	nop 
			9: R_SPARC_PLT32	foo
   c:	00 02 03 04 	(unimp|illtrap)  0x20304
