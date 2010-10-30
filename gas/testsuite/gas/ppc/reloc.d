#readelf: -r --wide
#name: reloc

Relocation section '\.rela\.data' at .* contains 2 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
0+08 .* R_PPC_ADDR32 .* y \+ f+fc
0+0c .* R_PPC_ADDR32 .* y \+ 0

Relocation section '\.rela\.data\.other' at .* contains 2 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
0+00 .* R_PPC_ADDR32 .* x \+ 0
0+04 .* R_PPC_ADDR32 .* x \+ f+fc
