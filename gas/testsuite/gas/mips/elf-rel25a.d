#source: elf-rel25.s
#as: -march=mips1 -mabi=32 -mno-shared
#objdump: -dr -Mgpr-names=numeric
#name: MIPS ELF reloc 25 -mno-shared

.*: * file format elf.*mips.*

Disassembly of section \.text:

0+00 <.*>:
.*:	3c1c0000 	lui	\$28,0x0
			.*: R_MIPS_HI16	__gnu_local_gp
.*:	279c0000 	addiu	\$28,\$28,0
			.*: R_MIPS_LO16	__gnu_local_gp
#pass
