#as: -Av9
#objdump: -dr --prefix-addresses
#name: v9branch1

.*: +file format .*sparc.*

Disassembly of section .text:
0x0+000000 brz  %o0, 0x0+01fffc
0x0+000004 nop 
	...
0x0+01fff8 nop 
0x0+01fffc nop 
	...
0x0+03fffc brz  %o0, 0x0+01fffc
0x0+040000 nop 
0x0+040004 bne  %icc, 0x0+140000
0x0+040008 nop 
	...
0x0+13fffc nop 
0x0+140000 nop 
	...
0x0+240000 bne  %icc, 0x0+140000
0x0+240004 nop 
