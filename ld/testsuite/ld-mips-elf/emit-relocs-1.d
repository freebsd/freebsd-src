#name: Emit relocs 1
#source: emit-relocs-1a.s -mabi=n32 -EB
#source: emit-relocs-1b.s -mabi=n32 -EB
#ld: -q -T emit-relocs-1.ld -melf32btsmipn32
#objdump: -sr

.*:     file format .*

RELOCATION RECORDS FOR \[\.data\]:
OFFSET   TYPE              VALUE *
00000000 R_MIPS_32         \.data
00000004 R_MIPS_32         \.data\+0x00001000
00000008 R_MIPS_32         \.merge1\+0x00000002
0000000c R_MIPS_32         \.merge2
00000010 R_MIPS_32         \.merge3
00000014 R_MIPS_32         \.merge3\+0x00000004
00000020 R_MIPS_32         \.data\+0x00000020
00000024 R_MIPS_32         \.data\+0x00001020
00000028 R_MIPS_32         \.merge1
0000002c R_MIPS_32         \.merge2\+0x00000002
00000030 R_MIPS_32         \.merge3\+0x00000008
00000034 R_MIPS_32         \.merge3\+0x00000004


Contents of section \.text:
 80000 03e00008 00000000 00000000 00000000  .*
Contents of section \.merge1:
 80400 666c7574 74657200                    flutter.*
Contents of section \.merge2:
 80800 74617374 696e6700                    tasting.*
Contents of section \.merge3:
 80c00 00000100 00000200 00000300           .*
Contents of section \.data:
 81000 00081000 00082000 00080402 00080800  .*
 81010 00080c00 00080c04 00000000 00000000  .*
 81020 00081020 00082020 00080400 00080802  .*
 81030 00080c08 00080c04 .*
