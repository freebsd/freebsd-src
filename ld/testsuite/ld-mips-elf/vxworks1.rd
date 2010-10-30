
Relocation section '\.rela\.dyn' at offset .* contains 1 entries:
 Offset     Info    Type            Sym.Value  Sym. Name \+ Addend
00081c00  .*7e R_MIPS_COPY       00081c00   dglobal \+ 0

Relocation section '\.rela\.plt' at offset .* contains 2 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00081400  .*7f R_MIPS_JUMP_SLOT  00080820   sglobal \+ 0
00081404  .*7f R_MIPS_JUMP_SLOT  00080840   foo \+ 0

Relocation section '\.rela\.text' at offset .* contains 3 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00080c00  .*04 R_MIPS_26         00080800   \.plt \+ 40
00080c08  .*04 R_MIPS_26         00080c18   sexternal \+ 0
00080c10  .*04 R_MIPS_26         00080800   \.plt \+ 20

Relocation section '\.rela\.data' at offset .* contains 3 entries:
 Offset     Info    Type            Sym.Value  Sym. Name \+ Addend
00081800  .*02 R_MIPS_32         00081800   .data \+ 0
00081804  .*02 R_MIPS_32         00081c00   .bss \+ 0
00081808  .*02 R_MIPS_32         00081804   dexternal \+ 0

Relocation section '\.rela\.plt\.unloaded' at offset .* contains 8 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00080800  .*05 R_MIPS_HI16       00081410   _GLOBAL_OFFSET_TABLE_ \+ 0
00080804  .*06 R_MIPS_LO16       00081410   _GLOBAL_OFFSET_TABLE_ \+ 0
00081400  .*02 R_MIPS_32         00080800   _PROCEDURE_LINKAGE_TAB.* \+ 18
00080820  .*05 R_MIPS_HI16       00081410   _GLOBAL_OFFSET_TABLE_ \+ fffffff0
00080824  .*06 R_MIPS_LO16       00081410   _GLOBAL_OFFSET_TABLE_ \+ fffffff0
00081404  .*02 R_MIPS_32         00080800   _PROCEDURE_LINKAGE_TAB.* \+ 38
00080840  .*05 R_MIPS_HI16       00081410   _GLOBAL_OFFSET_TABLE_ \+ fffffff4
00080844  .*06 R_MIPS_LO16       00081410   _GLOBAL_OFFSET_TABLE_ \+ fffffff4
