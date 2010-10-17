#objdump: -dr
#as: -mabi=32 -KPIC

.*:     file format .*

Disassembly of section \.text:

00000000 <.*>:
#
# Relocation agsinst .rodata.str1.1
#
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT16	\.rodata\.str1\.1
.*:	00000000 	nop
.*:	24840004 	addiu	a0,a0,4
			.*: R_MIPS_LO16	\.rodata\.str1\.1
#
# Relocation agsinst L2 + 2
#
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT16	L2
.*:	00000000 	nop
.*:	24840002 	addiu	a0,a0,2
			.*: R_MIPS_LO16	L2
#
# Relocation agsinst L2 - 0x4000 with 0x10000 added separately.
#
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT16	L2
.*:	3c010001 	lui	at,0x1
.*:	2421c000 	addiu	at,at,-16384
			.*: R_MIPS_LO16	L2
.*:	00812021 	addu	a0,a0,at
	\.\.\.
