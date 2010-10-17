#source: reloc-3a.s -mabi=32 -membedded-pic -EB
#source: reloc-3b.s -mabi=32 -membedded-pic -EB
#ld: --oformat=srec -Treloc-3.ld
#objdump: -D -mmips:4000 --endian=big

.*:     file format .*

Disassembly of section .*:

.* <.*>:
# .text2 - tstarta = 0x108000
# .text2 - tstartb = 0x098010
#
# Relocations against lda
#
.*:	3c040010 	lui	a0,0x10
.*:	2484fff8 	addiu	a0,a0,-8
.*:	3c040010 	lui	a0,0x10
.*:	24840008 	addiu	a0,a0,8
.*:	3c040011 	lui	a0,0x11
.*:	24848008 	addiu	a0,a0,-32760
.*:	3c040011 	lui	a0,0x11
.*:	2484fff8 	addiu	a0,a0,-8
.*:	3c040011 	lui	a0,0x11
.*:	24840018 	addiu	a0,a0,24
	\.\.\.
#
# Relocations against gd
#
.*:	3c04000f 	lui	a0,0xf
.*:	24840004 	addiu	a0,a0,4
.*:	3c04000f 	lui	a0,0xf
.*:	24840014 	addiu	a0,a0,20
.*:	3c040010 	lui	a0,0x10
.*:	24848014 	addiu	a0,a0,-32748
.*:	3c040010 	lui	a0,0x10
.*:	24840004 	addiu	a0,a0,4
.*:	3c040010 	lui	a0,0x10
.*:	24840024 	addiu	a0,a0,36
#
# Relocations against ldb
#
.*:	3c04000f 	lui	a0,0xf
.*:	24840010 	addiu	a0,a0,16
.*:	3c04000f 	lui	a0,0xf
.*:	24840020 	addiu	a0,a0,32
.*:	3c040010 	lui	a0,0x10
.*:	24848020 	addiu	a0,a0,-32736
.*:	3c040010 	lui	a0,0x10
.*:	24840010 	addiu	a0,a0,16
.*:	3c040010 	lui	a0,0x10
.*:	24840030 	addiu	a0,a0,48
	\.\.\.
#pass
