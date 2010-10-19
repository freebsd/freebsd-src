#as: -march=mips3 -mabi=64
#readelf: --relocs
#name: MIPS ELF reloc 24

Relocation section '\.rela\.text' .*:
.*
.* R_MIPS_GPREL32 * 0+00 foo \+ 0
 * Type2: R_MIPS_64 *
 * Type3: R_MIPS_NONE *
.* R_MIPS_GPREL32 * 0+00 \.text \+ 10
 * Type2: R_MIPS_64 *
 * Type3: R_MIPS_NONE *
