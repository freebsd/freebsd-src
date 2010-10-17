#objdump: -Dr
#source: immediate-006.s

.*:     file format elf32-d10v

Disassembly of section .text:

00000000 <foo>:
   0:	e0 00 00 08 	ldi.l	r0, 0x8
			2: R_D10V_16	.rodata
Disassembly of section .rodata:

00000000 <str0>:
   0:	6f 70 73 6f 	unknown	->	unknown.long	0x6f70736f
   4:	6c 6f 70 00 	unknown	->	ldb	r0, @r0

00000008 <str1>:
   8:	6d 6f 70 73 	unknown	->	unknown.long	0x6d6f7073
   c:	66 6c 6f 00 	unknown	->	unknown.long	0x666c6f00
