#objdump: -dr
#as: -mabi=n32 -mips3 -xgot -KPIC

.*:     file format .*

Disassembly of section \.text:

00000000 <.*>:
	\.\.\.
.*:	8f840000 	lw	a0,0\(gp\)
			.*: R_MIPS_GOT_PAGE	\.rodata\+0x8
.*:	dc840000 	ld	a0,0\(a0\)
			.*: R_MIPS_GOT_OFST	\.rodata\+0x8
	\.\.\.
