#as: -n32
#objdump: -Dr --prefix-addresses
#name: n32 consecutive unrelated relocations

.*:     file format .*mips.*

Disassembly of section .text:
	...
			0: R_MIPS_32	.text
Disassembly of section .data:
	...
			0: R_MIPS_32	.data\+0x4
Disassembly of section .reginfo:
	...
