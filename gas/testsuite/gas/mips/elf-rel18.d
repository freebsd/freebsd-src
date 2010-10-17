#readelf: --relocs
#as: -mabi=n32 -KPIC

Relocation section '\.rela\.text' at offset .* contains 4 entries:
 Offset     Info    Type            Sym.Value  Sym. Name \+ Addend
00000ed0  .* R_MIPS_CALL16     00000000   foo \+ 0
00000ed4  .* R_MIPS_JALR       00000000   foo \+ 0
00000edc  .* R_MIPS_CALL16     00000000   foo \+ 0
00000ee0  .* R_MIPS_JALR       00000000   foo \+ 0
