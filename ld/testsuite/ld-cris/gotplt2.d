#source: dso-2.s
#source: dsofnf.s
#source: gotrel1.s
#as: --pic --no-underscore --em=criself
#ld: -shared -m crislinux -z nocombreloc
#objdump: -sR

# Make sure we merge a PLT-specific entry (usually
# R_CRIS_JUMP_SLOT) with a GOT-specific entry (R_CRIS_GLOB_DAT)
# in a DSO.  It's ok: we make a round-trip to the PLT in the
# executable if it's referenced there, but that's still
# perceived as better than having an unnecessary PLT, dynamic
# reloc and lookup in the DSO.)

.*:     file format elf32-cris

DYNAMIC RELOCATION RECORDS
OFFSET   TYPE              VALUE 
00002230 R_CRIS_GLOB_DAT   dsofn

Contents of section .*
#...
Contents of section \.rela\.got:
 0188 30220000 0a080000 00000000           .*
Contents of section \.text:
 0194 5f1d0c00 30096f1d 0c000000 30090000  .*
 01a4 6f0d0c00 0000611a 6f3e88df ffff0000  .*
Contents of section \.dynamic:
 21b4 04000000 94000000 05000000 5c010000  .*
 21c4 06000000 cc000000 0a000000 2a000000  .*
 21d4 0b000000 10000000 07000000 88010000  .*
 21e4 08000000 0c000000 09000000 0c000000  .*
 21f4 00000000 00000000 00000000 00000000  .*
 2204 00000000 00000000 00000000 00000000  .*
 2214 00000000 00000000 00000000 00000000  .*
Contents of section \.got:
 2224 b4210000 00000000 00000000 00000000  .*
