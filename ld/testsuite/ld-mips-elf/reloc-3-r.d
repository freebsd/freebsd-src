#source: reloc-3a.s -mabi=32 -membedded-pic
#source: reloc-3b.s -mabi=32 -membedded-pic
#ld: -r
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

.* <.*>:
#
# Relocations against lda
#
.*:	3c04ffff 	lui	a0,0xffff
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	24847ffc 	addiu	a0,a0,32764
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	24848014 	addiu	a0,a0,-32748
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	2484001c 	addiu	a0,a0,28
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	24848014 	addiu	a0,a0,-32748
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	2484803c 	addiu	a0,a0,-32708
			.*: R_MIPS_GNU_REL_LO16	\.text2
	\.\.\.

.* <.*>:
#
# Relocations against gd
#
.*:	3c04ffff 	lui	a0,0xffff
			.*: R_MIPS_GNU_REL_HI16	gd
.*:	24847ff4 	addiu	a0,a0,32756
			.*: R_MIPS_GNU_REL_LO16	gd
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GNU_REL_HI16	gd
.*:	2484800c 	addiu	a0,a0,-32756
			.*: R_MIPS_GNU_REL_LO16	gd
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GNU_REL_HI16	gd
.*:	24840014 	addiu	a0,a0,20
			.*: R_MIPS_GNU_REL_LO16	gd
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GNU_REL_HI16	gd
.*:	2484800c 	addiu	a0,a0,-32756
			.*: R_MIPS_GNU_REL_LO16	gd
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GNU_REL_HI16	gd
.*:	24848034 	addiu	a0,a0,-32716
			.*: R_MIPS_GNU_REL_LO16	gd
#
# Relocations against ldb
#
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	2484802c 	addiu	a0,a0,-32724
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	24848044 	addiu	a0,a0,-32700
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	2484004c 	addiu	a0,a0,76
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	24848044 	addiu	a0,a0,-32700
			.*: R_MIPS_GNU_REL_LO16	\.text2
.*:	3c040001 	lui	a0,0x1
			.*: R_MIPS_GNU_REL_HI16	\.text2
.*:	2484806c 	addiu	a0,a0,-32660
			.*: R_MIPS_GNU_REL_LO16	\.text2
	\.\.\.
