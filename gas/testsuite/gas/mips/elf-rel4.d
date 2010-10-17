#objdump: --prefix-addresses -dr
#name: MIPS ELF reloc 4
#as: -32

.*: +file format.*

Disassembly of section .text:
0+000 <[^>]*> addiu	a0,gp,0
			0: R_MIPS_GPREL16	a
0+004 <[^>]*> addiu	a0,gp,4
			4: R_MIPS_GPREL16	a
0+008 <[^>]*> addiu	a0,gp,8
			8: R_MIPS_GPREL16	a
0+00c <[^>]*> addiu	a0,gp,12
			c: R_MIPS_GPREL16	a
