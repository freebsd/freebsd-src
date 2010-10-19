
Relocation section '\.rela\.plt' at offset .* contains 2 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
0008140c  .*16 R_ARM_JUMP_SLOT   00080810   sglobal \+ 0
00081410  .*16 R_ARM_JUMP_SLOT   00080828   foo \+ 0

Relocation section '\.rela\.text' at offset .* contains 3 entries:
 Offset     Info    Type            Sym.Value  Sym. Name \+ Addend
00080c00  .*01 R_ARM_PC24        00080800   \.plt \+ 20
00080c04  .*01 R_ARM_PC24        00080c0c   sexternal \+ fffffff8
00080c08  .*01 R_ARM_PC24        00080800   \.plt \+ 8

Relocation section '\.rela\.plt\.unloaded' at offset .* contains 5 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
0008080c  .*02 R_ARM_ABS32       00081400   _GLOBAL_OFFSET_TABLE_ \+ 0
00080818  .*02 R_ARM_ABS32       00081400   _GLOBAL_OFFSET_TABLE_ \+ c
0008140c  .*02 R_ARM_ABS32       00080800   _PROCEDURE_LINKAGE_TAB.* \+ 0
00080830  .*02 R_ARM_ABS32       00081400   _GLOBAL_OFFSET_TABLE_ \+ 10
00081410  .*02 R_ARM_ABS32       00080800   _PROCEDURE_LINKAGE_TAB.* \+ 0
