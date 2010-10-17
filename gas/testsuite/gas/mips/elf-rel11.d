#as: -march=mips3 -mabi=64
#readelf: --relocs
#name: MIPS ELF reloc 11

Relocation section '\.rela\.text' at offset .* contains 12 entries:
 *Offset * Info * Type * Sym\. Value * Sym\. Name \+ Addend
0+0000 * 0+..0000001d * R_MIPS_HIGHEST * 0+0000 * bar \+ 0
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0008 * 0+..0000001c * R_MIPS_HIGHER * 0+0000 * bar \+ 0
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0004 * 0+..00000005 * R_MIPS_HI16 * 0+0000 * bar \+ 0
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+000c * 0+..00000006 * R_MIPS_LO16 * 0+0000 * bar \+ 0
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0018 * 0+..0000001d * R_MIPS_HIGHEST * 0+0000 * bar \+ 12345678
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0020 * 0+..0000001c * R_MIPS_HIGHER * 0+0000 * bar \+ 12345678
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+001c * 0+..00000005 * R_MIPS_HI16 * 0+0000 * bar \+ 12345678
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0024 * 0+..00000006 * R_MIPS_LO16 * 0+0000 * bar \+ 12345678
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0030 * 0+..0000001d * R_MIPS_HIGHEST * 0+0000 * \.data \+ 10
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0034 * 0+..0000001c * R_MIPS_HIGHER * 0+0000 * \.data \+ 10
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+003c * 0+..00000005 * R_MIPS_HI16 * 0+0000 * \.data \+ 10
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
0+0044 * 0+..00000006 * R_MIPS_LO16 * 0+0000 * \.data \+ 10
 * Type2: R_MIPS_NONE *
 * Type3: R_MIPS_NONE *
#pass
