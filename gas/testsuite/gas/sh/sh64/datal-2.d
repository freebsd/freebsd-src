#as: --abi=32
#objdump: -sr
#source: datal-2.s
#name: DataLabel redundant local use, SHcompact

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET  *TYPE  *VALUE 
0+08 R_SH_DIR32        \.rodata
0+0c R_SH_DIR32        myrodata2
0+10 R_SH_DIR32        \.text
0+14 R_SH_DIR32        \.text
0+18 R_SH_DIR32        \.text
0+1c R_SH_DIR32        \.text


RELOCATION RECORDS FOR \[\.data\]:
OFFSET  *TYPE  *VALUE 
0+00 R_SH_DIR32        myrodata2
0+04 R_SH_DIR32        \.data
0+08 R_SH_DIR32        \.data
0+0c R_SH_DIR32        foo2
0+10 R_SH_DIR32        foo3
0+14 R_SH_DIR32        \.text
0+18 R_SH_DIR32        \.text


RELOCATION RECORDS FOR \[\.rodata\]:
OFFSET  *TYPE  *VALUE 
0+00 R_SH_DIR32        \.data
0+04 R_SH_DIR32        \.data
0+08 R_SH_DIR32        \.rodata
0+0c R_SH_DIR32        \.rodata


Contents of section \.text:
 0000 c701c70d 00090009 00000004 00000014  .*
 0010 00000002 0000002e 00000018 00000030  .*
Contents of section \.data:
 0000 00000000 00000004 0000001c 00000000  .*
 0010 00000014 00000002 00000018           .*
Contents of section \.rodata:
 0000 00000010 0000004c 00000008 00000020  .*
