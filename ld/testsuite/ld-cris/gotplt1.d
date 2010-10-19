#source: dso-2.s
#source: dsofnf2.s
#source: gotrel1.s
#as: --pic --no-underscore --em=criself
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
00082280 R_CRIS_JUMP_SLOT  dsofn

Contents of section .*
#...
Contents of section \.rela\.plt:
 80190 80220800 0b040000 00000000           .*
Contents of section \.plt:
 8019c fce17e7e 7f0d7822 0800307a 7f0d7c22  .*
 801ac 08003009 7f0d8022 08003009 3f7e0000  .*
 801bc 00002ffe d8ffffff                    .*
Contents of section \.text:
 801c4 5f1d0c00 30096f1d 0c000000 30090000  .*
 801d4 6f0d1000 0000611a 6f2eb001 08000000  .*
 801e4 6f3e70df ffff0000                    .*
Contents of section \.dynamic:
 821ec 01000000 01000000 04000000 e4000800  .*
 821fc 05000000 5c010800 06000000 0c010800  .*
 8220c 0a000000 32000000 0b000000 10000000  .*
 8221c 15000000 00000000 03000000 74220800  .*
 8222c 02000000 0c000000 14000000 07000000  .*
 8223c 17000000 90010800 00000000 00000000  .*
 8224c 00000000 00000000 00000000 00000000  .*
 8225c 00000000 00000000 00000000 00000000  .*
 8226c 00000000 00000000                    .*
Contents of section \.got:
 82274 ec210800 00000000 00000000 b8010800  .*
 82284 b0010800                             .*
