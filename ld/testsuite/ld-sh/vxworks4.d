#source: vxworks4a.s
#source: vxworks4b.s
#ld: -shared -Tvxworks1.ld
#target: sh-*-vxworks
#readelf: --relocs

Relocation section '\.rela\.dyn' at offset .* contains 3 entries:
 Offset     Info    Type            Sym\.Value  Sym\. Name \+ Addend
00081810  000000a5 R_SH_RELATIVE                                0008181c
00081814  .*01 R_SH_DIR32        00000000   global \+ 1234
00081818  .*02 R_SH_REL32        00000000   global \+ 1234
