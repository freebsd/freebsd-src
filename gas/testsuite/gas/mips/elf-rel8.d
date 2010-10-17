#as: -march=mips2 -mabi=32
#objdump: -M gpr-names=numeric -dr
#name: MIPS ELF reloc 8

.*:     file format .*

Disassembly of section \.text:

0+00 <foo>:
   0:	3c040000 	lui	\$4,0x0
			0: R_MIPS_HI16	gvar
   4:	24840000 	addiu	\$4,\$4,0
			4: R_MIPS_LO16	gvar
   8:	8ca40000 	lw	\$4,0\(\$5\)
			8: R_MIPS_LO16	gvar
   c:	8fc40002 	lw	\$4,2\(\$30\)
  10:	3c040000 	lui	\$4,0x0
			10: R_MIPS_CALL_HI16	gfunc
  14:	009c2021 	addu	\$4,\$4,\$28
  18:	8c990000 	lw	\$25,0\(\$4\)
			18: R_MIPS_CALL_LO16	gfunc
  1c:	3c040000 	lui	\$4,0x0
			1c: R_MIPS_GOT_HI16	gvar
  20:	009c2021 	addu	\$4,\$4,\$28
  24:	8c850000 	lw	\$5,0\(\$4\)
			24: R_MIPS_GOT_LO16	gvar
  28:	8f840000 	lw	\$4,0\(\$28\)
			28: R_MIPS_GOT16	\.data
  2c:	a0850000 	sb	\$5,0\(\$4\)
			2c: R_MIPS_LO16	\.data
  30:	3c040000 	lui	\$4,0x0
			30: R_MIPS_CALL_HI16	gfunc
  34:	24840000 	addiu	\$4,\$4,0
			34: R_MIPS_CALL_LO16	gfunc
  38:	3c040000 	lui	\$4,0x0
			38: R_MIPS_GOT_HI16	gvar
  3c:	24840000 	addiu	\$4,\$4,0
			3c: R_MIPS_GOT_LO16	gvar
  40:	8f840000 	lw	\$4,0\(\$28\)
			40: R_MIPS_GOT16	\.data
  44:	24840000 	addiu	\$4,\$4,0
			44: R_MIPS_LO16	\.data
  48:	8f990000 	lw	\$25,0\(\$28\)
			48: R_MIPS_CALL16	gfunc
  4c:	27840000 	addiu	\$4,\$28,0
			4c: R_MIPS_CALL16	gfunc
  50:	8f840000 	lw	\$4,0\(\$28\)
			50: R_MIPS_GOT_DISP	gvar
  54:	27840000 	addiu	\$4,\$28,0
			54: R_MIPS_GOT_DISP	gvar
  58:	8f840000 	lw	\$4,0\(\$28\)
			58: R_MIPS_GPREL16	gvar
  5c:	af840000 	sw	\$4,0\(\$28\)
			5c: R_MIPS_GPREL16	gvar
  60:	27840000 	addiu	\$4,\$28,0
			60: R_MIPS_GPREL16	gvar
	\.\.\.
