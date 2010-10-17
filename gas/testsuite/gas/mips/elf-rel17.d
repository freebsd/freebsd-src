#objdump: -dr
#as: -mabi=32

.*:     file format .*

Disassembly of section \.text:

00000000 <.*>:
.*:	3c040000 	lui	a0,0x0
			.*: R_MIPS_HI16	x
.*:	24840000 	addiu	a0,a0,0
			.*: R_MIPS_LO16	x
	\.\.\.
