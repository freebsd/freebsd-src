#objdump: -dr --prefix-addresses
#name: MIPS ELF reloc 6
#as: -32

.*: +file format elf.*mips.*

Disassembly of section \.text:
0+00 <.*> lb	v0,0\(v1\)
			0: R_MIPS16_GPREL	bar
0+04 <.*> lb	v0,1\(v1\)
			4: R_MIPS16_GPREL	bar
0+08 <.*> lb	v0,4660\(v1\)
			8: R_MIPS16_GPREL	bar
0+0c <[^>]*> nop
0+0e <[^>]*> nop
