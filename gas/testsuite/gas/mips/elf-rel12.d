#as: -march=mips1 -mabi=32
#readelf: --relocs
#name: MIPS ELF reloc 12

Relocation section '\.rel\.text' at offset .* contains 4 entries:
 *Offset * Info * Type * Sym\.Value * Sym\. Name
0+0004 * 0+..05 * R_MIPS_HI16 * 0+0000 * l1
0+0008 * 0+..06 * R_MIPS_LO16 * 0+0000 * l1
0+0000 * 0+..05 * R_MIPS_HI16 * 0+0004 * l2
0+000c * 0+..06 * R_MIPS_LO16 * 0+0004 * l2
#pass
