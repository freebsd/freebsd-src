#objdump: --prefix-addresses -dr --show-raw-insn -mmips:4000
#name: MIPS empic3 (global, positive)
#as: -mabi=o64 -membedded-pic -mips3

# Check PC-relative HI/LO relocs relocs for -membedded-pic when HI and
# LO are split over a 32K boundary.

.*: +file format elf.*mips.*

Disassembly of section .text:
	...
	...
0000fffc <[^>]*> 3c020005 	lui	v0,0x5
[ 	]*fffc: R_MIPS_GNU_REL_HI16	.text
00010000 <[^>]*> 64428000 	daddiu	v0,v0,-32768
[ 	]*10000: R_MIPS_GNU_REL_LO16	.text
	...
00017ffc <[^>]*> 3c020005 	lui	v0,0x5
[ 	]*17ffc: R_MIPS_GNU_REL_HI16	.text
00018000 <[^>]*> 64420000 	daddiu	v0,v0,0
[ 	]*18000: R_MIPS_GNU_REL_LO16	.text
	...
0001fffc <[^>]*> 3c020006 	lui	v0,0x6
[ 	]*1fffc: R_MIPS_GNU_REL_HI16	.text
00020000 <[^>]*> 0043102d 	daddu	v0,v0,v1
00020004 <[^>]*> 64428004 	daddiu	v0,v0,-32764
[ 	]*20004: R_MIPS_GNU_REL_LO16	.text
	...
00027ffc <[^>]*> 3c020006 	lui	v0,0x6
[ 	]*27ffc: R_MIPS_GNU_REL_HI16	.text
00028000 <[^>]*> 0043102d 	daddu	v0,v0,v1
00028004 <[^>]*> 64420004 	daddiu	v0,v0,4
[ 	]*28004: R_MIPS_GNU_REL_LO16	.text
	...
0002fff8 <[^>]*> 3c020007 	lui	v0,0x7
[ 	]*2fff8: R_MIPS_GNU_REL_HI16	.text
0002fffc <[^>]*> 0043102d 	daddu	v0,v0,v1
00030000 <[^>]*> 64428000 	daddiu	v0,v0,-32768
[ 	]*30000: R_MIPS_GNU_REL_LO16	.text
	...
00037ff8 <[^>]*> 3c020007 	lui	v0,0x7
[ 	]*37ff8: R_MIPS_GNU_REL_HI16	.text
00037ffc <[^>]*> 0043102d 	daddu	v0,v0,v1
00038000 <[^>]*> 64420000 	daddiu	v0,v0,0
[ 	]*38000: R_MIPS_GNU_REL_LO16	.text
	...
	...
