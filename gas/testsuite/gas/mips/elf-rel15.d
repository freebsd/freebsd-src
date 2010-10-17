#objdump: -dr
#as: -mabi=n32 -mips3

.*:     file format .*

Disassembly of section \.text:

00000000 <foo>:
   0:	3c020000 	lui	v0,0x0
			0: R_MIPS_GPREL16	\.text
			0: R_MIPS_SUB	\*ABS\*
			0: R_MIPS_HI16	\*ABS\*
   4:	23bdffe4 	addi	sp,sp,-28
	...
