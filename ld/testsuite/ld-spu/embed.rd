
Relocation section '\.rela\.rodata\.speelf' at .* contains 3 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
00000184  00000601 R_PPC_ADDR32           00000000   main \+ 0
000001a4  00000901 R_PPC_ADDR32           00000000   foo \+ 0
000001b4  00000701 R_PPC_ADDR32           00000000   blah \+ 0

Relocation section '\.rela\.data' at .* contains 2 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
00000004  00000201 R_PPC_ADDR32           00000000   \.rodata\.speelf \+ 0
00000008  00000401 R_PPC_ADDR32           00000000   \.data\.spetoe \+ 0

Relocation section '\.rela\.data\.spetoe' at .* contains 2 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
00000004  00000201 R_PPC_ADDR32           00000000   \.rodata\.speelf \+ 0
00000014  00000a01 R_PPC_ADDR32           00000000   bar \+ 0
