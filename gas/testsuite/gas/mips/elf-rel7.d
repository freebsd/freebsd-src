#objdump: -dr --prefix-addresses
#name: MIPS ELF reloc 7
#as: -32

.*: +file format elf.*mips.*

Disassembly of section \.text:
0+00 <.*> lui	a0,0x0
			0: R_MIPS_HI16	bar
0+04 <.*> lw	a0,0\(a0\)
			4: R_MIPS_LO16	bar
0+08 <.*> lui	a0,0x0
			8: R_MIPS_HI16	bar
0+0c <.*> lw	a0,4\(a0\)
			c: R_MIPS_LO16	bar
0+10 <.*> lui	a0,0x0
			10: R_MIPS_HI16	bar
0+14 <.*> lw	a0,8\(a0\)
			14: R_MIPS_LO16	bar
0+18 <.*> lui	a0,0x0
			18: R_MIPS_HI16	frob
0+1c <.*> lw	a0,0\(a0\)
			1c: R_MIPS_LO16	frob
0+20 <.*> lui	a0,0x0
			20: R_MIPS_HI16	frob
0+24 <.*> lw	a0,4\(a0\)
			24: R_MIPS_LO16	frob
0+28 <.*> lui	a0,0x0
			28: R_MIPS_HI16	frob
0+2c <.*> lw	a0,16\(a0\)
			2c: R_MIPS_LO16	frob
#pass
