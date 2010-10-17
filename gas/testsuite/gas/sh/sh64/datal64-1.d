#as: --abi=64
#objdump: -sr
#source: datal-1.s
#name: DataLabel redundant local use, SHmedia 64-bit ABI

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+20 R_SH_IMM_MEDLOW16  \.data\+0x0+3a
0+24 R_SH_IMM_LOW16    myrodata3
0+28 R_SH_IMM_LOW16    \.rodata\+0x0+10
0+2c R_SH_IMM_LOW16    \.rodata\+0x0+3a
0+00 R_SH_IMM_HI16     \.data\+0x0+4
0+04 R_SH_IMM_MEDHI16  \.data\+0x0+4
0+08 R_SH_IMM_MEDLOW16  \.data\+0x0+4
0+0c R_SH_IMM_LOW16    \.data\+0x0+4
0+10 R_SH_IMM_HI16     \.data\+0x0+32
0+14 R_SH_IMM_MEDHI16  \.data\+0x0+32
0+18 R_SH_IMM_MEDLOW16  \.data\+0x0+32
0+1c R_SH_IMM_LOW16    \.data\+0x0+32

RELOCATION RECORDS FOR \[\.data\]:
OFFSET           TYPE              VALUE 
0+00 R_SH_DIR32        \.rodata
0+04 R_SH_DIR32        \.rodata
0+08 R_SH_DIR32        \.data
0+0c R_SH_DIR32        \.data
0+10 R_SH_DIR32        \.data
0+14 R_SH_DIR32        myrodata3
0+18 R_SH_DIR32        foo6

RELOCATION RECORDS FOR \[\.rodata\]:
OFFSET           TYPE              VALUE 
0+00 R_SH_DIR32        \.data
0+04 R_SH_DIR32        \.data
0+08 R_SH_DIR32        \.rodata
0+0c R_SH_DIR32        \.rodata
0+10 R_SH_DIR32        \.rodata

Contents of section \.text:
 0000 cc000030 c8000030 c8000030 c8000030  .*
 0010 cc000030 c8000030 c8000030 c8000030  .*
 0020 cc000030 cc0002d0 cc0002d0 cc0002d0  .*
Contents of section \.data:
 0000 00000004 00000026 00000004 0000000c  .*
 0010 00000038 00000000 0000002a           .*
Contents of section \.rodata:
 0000 00000010 0000004c 00000008 00000020  .*
 0010 00000104                             .*
