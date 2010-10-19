#objdump: -rs
#name: i386 secrel reloc

.*: +file format pe-i386

RELOCATION RECORDS FOR \[\.data\]:
OFFSET   TYPE              VALUE 
00000024 secrel32          \.text
00000029 secrel32          \.text
0000002e secrel32          \.text
00000033 secrel32          \.text
00000044 secrel32          \.data
00000049 secrel32          \.data
0000004e secrel32          \.data
00000053 secrel32          \.data
00000064 secrel32          \.rdata
00000069 secrel32          \.rdata
0000006e secrel32          \.rdata
00000073 secrel32          \.rdata
00000084 secrel32          ext24
00000089 secrel32          ext2d
0000008e secrel32          ext36
00000093 secrel32          ext3f


Contents of section \.text:
 0000 3e3e3e3e 3c3c3c3c 3e3e3e3e 3e3c3c3c  >>>><<<<>>>>><<<
 0010 3e3e3e3e 3e3e3c3c 3e3e3e3e 3e3e3e3c  >>>>>><<>>>>>>><
Contents of section \.data:
 0000 3e3e3e3e 3c3c3c3c 3e3e3e3e 3e3c3c3c  >>>><<<<>>>>><<<
 0010 3e3e3e3e 3e3e3c3c 3e3e3e3e 3e3e3e3c  >>>>>><<>>>>>>><
 0020 3e3e3e3e 04000000 110d0000 00111600  >>>>............
 0030 0000111f 00000011 3c3c3c3c 3c3c3c3c  ........<<<<<<<<
 0040 3e3e3e3e 04000000 110d0000 00111600  >>>>............
 0050 0000111f 00000011 3c3c3c3c 3c3c3c3c  ........<<<<<<<<
 0060 3e3e3e3e 04000000 110d0000 00111600  >>>>............
 0070 0000111f 00000011 3c3c3c3c 3c3c3c3c  ........<<<<<<<<
 0080 3e3e3e3e 00000000 11000000 00110000  >>>>............
 0090 00001100 00000011 3c3c3c3c 3c3c3c3c  ........<<<<<<<<
Contents of section \.rdata:
 0000 3e3e3e3e 3c3c3c3c 3e3e3e3e 3e3c3c3c  >>>><<<<>>>>><<<
 0010 3e3e3e3e 3e3e3c3c 3e3e3e3e 3e3e3e3c  >>>>>><<>>>>>>><
 0020 3e3e3e3e 00000000 00000000 00000000  >>>>............
