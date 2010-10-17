#source: dso-2.s
#source: dsofnf2.s
#source: gotrel1.s
#as: --pic --no-underscore
#ld: -m crislinux tmpdir/libdso-1.so
#objdump: -sR

# Make sure we don't merge a PLT-specific entry
# (R_CRIS_JUMP_SLOT) with a non-PLT-GOT-specific entry
# (R_CRIS_GLOB_DAT) in an executable, since they may have
# different contents there.  (If we merge them in a DSO it's ok:
# we make a round-trip to the PLT in the executable if it's
# referenced there, but that's still perceived as better than
# having an unnecessary PLT, dynamic reloc and lookup in the
# DSO.)  In the executable, the GOT contents for the non-PLT
# reloc should be constant.

.*:     file format elf32-cris

DYNAMIC RELOCATION RECORDS
OFFSET   TYPE              VALUE 
000822d4 R_CRIS_JUMP_SLOT  dsofn

Contents of section .*
#...
Contents of section \.rela\.plt:
 801d8 d4220800 0b060000 00000000           .*
Contents of section \.plt:
 801e4 fce17e7e 7f0dcc22 0800307a 7f0dd022  .*
 801f4 08003009 7f0dd422 08003009 3f7e0000  .*
 80204 00002ffe d8ffffff                    .*
Contents of section \.text:
 8020c 5f1d0c00 30096f1d 0c000000 30090000  .*
 8021c 6f0d1000 0000611a 6f2ef801 08000000  .*
 8022c 6f3e64df ffff0000                    .*
Contents of section \.dynamic:
 82240 01000000 01000000 04000000 e4000800  .*
 82250 05000000 84010800 06000000 14010800  .*
 82260 0a000000 51000000 0b000000 10000000  .*
 82270 15000000 00000000 03000000 c8220800  .*
 82280 02000000 0c000000 14000000 07000000  .*
 82290 17000000 d8010800 00000000 00000000  .*
 822a0 00000000 00000000 00000000 00000000  .*
 822b0 00000000 00000000 00000000 00000000  .*
 822c0 00000000 00000000                    .*
Contents of section \.got:
 822c8 40220800 00000000 00000000 00020800  .*
 822d8 f8010800                             .*
