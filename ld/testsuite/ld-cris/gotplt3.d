#source: dso-2.s
#source: dsofnf.s
#source: gotrel1.s
#source: dso-1.s
#as: --pic --no-underscore --em=criself
#ld: -shared -m crislinux -z nocombreloc
#objdump: -sR

# Like gotplt2, but make sure we merge right when we have a
# definition of the function too.

.*:     file format elf32-cris

DYNAMIC RELOCATION RECORDS
OFFSET   TYPE              VALUE 
00002234 R_CRIS_GLOB_DAT   dsofn

Contents of section .*
#...
Contents of section \.rela\.got:
 0188 34220000 0a080000 00000000           .*
Contents of section \.text:
 0194 5f1d0c00 30096f1d 0c000000 30090000  .*
 01a4 6f0d0c00 0000611a 6f3e84df ffff0000  .*
 01b4 0f050000                             .*
Contents of section \.dynamic:
 21b8 04000000 94000000 05000000 5c010000  .*
 21c8 06000000 cc000000 0a000000 2a000000  .*
 21d8 0b000000 10000000 07000000 88010000  .*
 21e8 08000000 0c000000 09000000 0c000000  .*
 21f8 00000000 00000000 00000000 00000000  .*
 2208 00000000 00000000 00000000 00000000  .*
 2218 00000000 00000000 00000000 00000000  .*
Contents of section \.got:
 2228 b8210000 00000000 00000000 00000000  .*
