#as: -march=mips2 -mabi=32 -KPIC
#readelf: --relocs
#name: MIPS ELF reloc 13

Relocation section '\.rel\.text' at offset .* contains 9 entries:
 *Offset * Info * Type * Sym\.Value * Sym\. Name
0+0000 * 0+..09 * R_MIPS_GOT16 * 0+0000 * \.data
0+0014 * 0+..06 * R_MIPS_LO16 * 0+0000 * \.data
0+0010 * 0+..09 * R_MIPS_GOT16 * 0+0000 * \.data
0+0018 * 0+..06 * R_MIPS_LO16 * 0+0000 * \.data
# The next two lines could be in either order.
0+000c * 0+..09 * R_MIPS_GOT16 * 0+0000 * \.rodata
0+0008 * 0+..09 * R_MIPS_GOT16 * 0+0000 * \.rodata
0+001c * 0+..06 * R_MIPS_LO16 * 0+0000 * \.rodata
0+0004 * 0+..09 * R_MIPS_GOT16 * 0+0000 * \.bss
0+0020 * 0+..06 * R_MIPS_LO16 * 0+0000 * \.bss
#pass
