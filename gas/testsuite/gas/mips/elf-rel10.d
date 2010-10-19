#as: -march=mips3 -mabi=n32 -KPIC
#readelf: --relocs
#name: MIPS ELF reloc 10

Relocation section '\.rela\.text' at offset .* contains 22 entries:
 *Offset * Info * Type * Sym\.Value * Sym\. Name \+ Addend
0+0000 * 0+..07 * R_MIPS_GPREL16 * 0+0000 * foo \+ 0
0+0000 * 0+0018 * R_MIPS_SUB * 0+0000
0+0000 * 0+0005 * R_MIPS_HI16 * 0+0000
0+0004 * 0+..07 * R_MIPS_GPREL16 * 0+0000 * foo \+ 0
0+0004 * 0+0018 * R_MIPS_SUB * 0+0000
0+0004 * 0+0006 * R_MIPS_LO16 * 0+0000
0+000c * 0+..07 * R_MIPS_GPREL16 * 0+0000 * \.text \+ c
0+000c * 0+0018 * R_MIPS_SUB * 0+0000
0+000c * 0+0005 * R_MIPS_HI16 * 0+0000
0+0010 * 0+..07 * R_MIPS_GPREL16 * 0+0000 * \.text \+ c
0+0010 * 0+0018 * R_MIPS_SUB * 0+0000
0+0010 * 0+0006 * R_MIPS_LO16 * 0+0000
0+0018 * 0+..14 * R_MIPS_GOT_PAGE * 0+0000 * foo \+ 0
0+001c * 0+..15 * R_MIPS_GOT_OFST * 0+0000 * foo \+ 0
0+0020 * 0+..14 * R_MIPS_GOT_PAGE * 0+0000 * foo \+ 1234
0+0024 * 0+..15 * R_MIPS_GOT_OFST * 0+0000 * foo \+ 1234
0+0028 * 0+..14 * R_MIPS_GOT_PAGE * 0+0000 * \.text \+ c
0+002c * 0+..15 * R_MIPS_GOT_OFST * 0+0000 * \.text \+ c
0+0030 * 0+..14 * R_MIPS_GOT_PAGE * 0+0000 * \.text \+ 33221d
0+0034 * 0+..15 * R_MIPS_GOT_OFST * 0+0000 * \.text \+ 33221d
0+0038 * 0+..14 * R_MIPS_GOT_PAGE * 0+0000 * \.text \+ 18
0+003c * 0+..15 * R_MIPS_GOT_OFST * 0+0000 * \.text \+ 18
#pass
