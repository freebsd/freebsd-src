#as: -march=mips1 -mabi=32
#objdump: -dr -Mgpr-names=numeric
#name: MIPS ELF reloc 25

.*: * file format elf.*mips.*

Disassembly of section \.text:

0+00 <.*>:
.*:	3c1c0000 	lui	\$28,0x0
			.*: R_MIPS_HI16	_gp_disp
.*:	279c0000 	addiu	\$28,\$28,0
			.*: R_MIPS_LO16	_gp_disp
.*:	0399e021 	addu	\$28,\$28,\$25
#pass
