
Relocation section '\.rela\.plt' at offset .* contains 2 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
0009040c  .*15 R_PPC_JMP_SLOT    00080820   sglobal \+ 0
00090410  .*15 R_PPC_JMP_SLOT    00080840   foo \+ 0

Relocation section '\.rela\.text' at offset .* contains 3 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00080c00  .*12 R_PPC_PLTREL24    00080800   \.plt \+ 40
00080c04  .*12 R_PPC_PLTREL24    00080c0c   sexternal \+ 0
00080c08  .*12 R_PPC_PLTREL24    00080800   \.plt \+ 20

Relocation section '\.rela\.plt\.unloaded' at offset .* contains 8 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00080802  .*06 R_PPC_ADDR16_HA   00090400   _GLOBAL_OFFSET_TABLE_ \+ 0
00080806  .*04 R_PPC_ADDR16_LO   00090400   _GLOBAL_OFFSET_TABLE_ \+ 0
00080822  .*06 R_PPC_ADDR16_HA   00090400   _GLOBAL_OFFSET_TABLE_ \+ c
00080826  .*04 R_PPC_ADDR16_LO   00090400   _GLOBAL_OFFSET_TABLE_ \+ c
0009040c  .*01 R_PPC_ADDR32      00080800   _PROCEDURE_LINKAGE_TAB.* \+ 30
00080842  .*06 R_PPC_ADDR16_HA   00090400   _GLOBAL_OFFSET_TABLE_ \+ 10
00080846  .*04 R_PPC_ADDR16_LO   00090400   _GLOBAL_OFFSET_TABLE_ \+ 10
00090410  .*01 R_PPC_ADDR32      00080800   _PROCEDURE_LINKAGE_TAB.* \+ 50
