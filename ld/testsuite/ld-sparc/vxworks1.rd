
Relocation section '\.rela\.plt' at offset .* contains 2 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
0009040c  .*15 R_SPARC_JMP_SLOT  00080814   sglobal \+ 0
00090410  .*15 R_SPARC_JMP_SLOT  00080834   foo \+ 0

Relocation section '\.rela\.text' at offset .* contains 3 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00080c04  .*07 R_SPARC_WDISP30   00080800   \.plt \+ 34
00080c0c  .*07 R_SPARC_WDISP30   00080c24   sexternal \+ 0
00080c14  .*07 R_SPARC_WDISP30   00080800   \.plt \+ 14

Relocation section '\.rela\.plt\.unloaded' at offset .* contains 8 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00080800  .*09 R_SPARC_HI22      00090400   _GLOBAL_OFFSET_TABLE_ \+ 8
00080804  .*0c R_SPARC_LO10      00090400   _GLOBAL_OFFSET_TABLE_ \+ 8
00080814  .*09 R_SPARC_HI22      00090400   _GLOBAL_OFFSET_TABLE_ \+ c
00080818  .*0c R_SPARC_LO10      00090400   _GLOBAL_OFFSET_TABLE_ \+ c
0009040c  .*03 R_SPARC_32        00080800   _PROCEDURE_LINKAGE_TAB.* \+ 28
00080834  .*09 R_SPARC_HI22      00090400   _GLOBAL_OFFSET_TABLE_ \+ 10
00080838  .*0c R_SPARC_LO10      00090400   _GLOBAL_OFFSET_TABLE_ \+ 10
00090410  .*03 R_SPARC_32        00080800   _PROCEDURE_LINKAGE_TAB.* \+ 48
