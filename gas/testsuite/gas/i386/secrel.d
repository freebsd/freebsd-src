#objdump: -rs
#name: i386 secrel reloc

.*: +file format pe-i386

RELOCATION RECORDS FOR \[\.data\]:
OFFSET[ 	]+TYPE[ 	]+VALUE 
0+24 secrel32          \.text
0+29 secrel32          \.text
0+2e secrel32          \.text
0+33 secrel32          \.text
0+44 secrel32          \.data
0+49 secrel32          \.data
0+4e secrel32          \.data
0+53 secrel32          \.data
0+64 secrel32          \.rdata
0+69 secrel32          \.rdata
0+6e secrel32          \.rdata
0+73 secrel32          \.rdata
0+84 secrel32          ext24
0+89 secrel32          ext2d
0+8e secrel32          ext36
0+93 secrel32          ext3f


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
