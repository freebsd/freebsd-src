#as: -mips32 -EL -KPIC
#readelf: --relocs
#name: MIPS ELF reloc 26

Relocation section '\.rel\.pdr' .*
 *Offset.*
00.*

Relocation section '\.rel\.text\.foo' at offset .* contains 11 entries:
 *Offset * Info * Type * Sym\.Value * Sym\. Name
0+000 * .+ * R_MIPS_HI16 * 0+0 * _gp_disp
0+004 * .+ * R_MIPS_LO16 * 0+0 * _gp_disp
0+014 * .+ * R_MIPS_GOT16 * 0+0 * \$LC28
0+01c * .+ * R_MIPS_LO16 * 0+0 * \$LC28
0+020 * .+ * R_MIPS_CALL16 * 0+0 * bar
0+030 * .+ * R_MIPS_PC16 * 0+0 * \$L846
0+034 * .+ * R_MIPS_GOT16 * 0+0 * \$LC27
0+038 * .+ * R_MIPS_PC16 * 0+0 * \$L848
0+048 * .+ * R_MIPS_PC16 * 0+0 * \$L925
0+010 * .+ * R_MIPS_GOT16 * 0+0 * \.rodata\.foo
0+05c * .+ * R_MIPS_LO16 * 0+0 * \.rodata\.foo
#pass
