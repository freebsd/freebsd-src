#objdump: -dr
#name: MIPS VR4111
#as: -march=vr4111

.*: +file format .*mips.*

Disassembly of section \.text:
0+000 <\.text>:
 + 0:	00850029 	dmadd16	a0,a1
	\.\.\.
 + c:	00a60028 	madd16	a1,a2
