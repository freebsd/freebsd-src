#as: -march=mips2 -mabi=32 -KPIC
#readelf: --relocs
#name: MIPS ELF reloc 20

Relocation section '\.rel\.text' at offset .* contains 8 entries:
 *Offset * Info * Type * Sym\.Value * Sym\. Name
0+0000 * 0+..05 * R_MIPS_HI16 * 0+0000 * foo
0+0010 * 0+..06 * R_MIPS_LO16 * 0+0000 * foo
0+0004 * 0+..05 * R_MIPS_HI16 * 0+0000 * foo
0+0014 * 0+..06 * R_MIPS_LO16 * 0+0000 * foo
0+000c * 0+..05 * R_MIPS_HI16 * 0+0000 * \.bss
0+0018 * 0+..06 * R_MIPS_LO16 * 0+0000 * \.bss
0+0008 * 0+..05 * R_MIPS_HI16 * 0+0000 * \.bss
0+001c * 0+..06 * R_MIPS_LO16 * 0+0000 * \.bss
#pass
