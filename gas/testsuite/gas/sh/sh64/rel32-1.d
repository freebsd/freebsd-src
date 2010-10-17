#as: --abi=32
#objdump: -sr
#source: rel-1.s
#name: MOVI: PC-relative relocs, 32-bit ABI.

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET  *TYPE  *VALUE 
0+08 R_SH_IMM_LOW16_PCREL  \.data\+0x0+8
0+0c R_SH_IMM_LOW16_PCREL  \.data\+0x0+c
0+10 R_SH_IMM_MEDLOW16_PCREL  \.data\+0x0+10
0+1c R_SH_IMM_LOW16_PCREL  \.data\+0x0+28
0+20 R_SH_IMM_LOW16_PCREL  \.data\+0x0+28
0+24 R_SH_IMM_MEDLOW16_PCREL  \.data\+0x0+24
0+30 R_SH_IMM_LOW16_PCREL  \.othertext\+0x0+9
0+34 R_SH_IMM_LOW16_PCREL  \.othertext\+0x0+d
0+38 R_SH_IMM_MEDLOW16_PCREL  \.othertext\+0x0+11
0+44 R_SH_IMM_LOW16_PCREL  \.othertext\+0x0+29
0+48 R_SH_IMM_LOW16_PCREL  \.othertext\+0x0+29
0+4c R_SH_IMM_MEDLOW16_PCREL  \.othertext\+0x0+25
0+58 R_SH_IMM_LOW16_PCREL  extern2
0+5c R_SH_IMM_LOW16_PCREL  extern3
0+60 R_SH_IMM_MEDLOW16_PCREL  extern4
0+6c R_SH_IMM_LOW16_PCREL  extern6\+0x0+10
0+70 R_SH_IMM_LOW16_PCREL  extern7\+0x0+c
0+74 R_SH_IMM_MEDLOW16_PCREL  extern8\+0x0+4
0+80 R_SH_IMM_LOW16_PCREL  gdata2
0+84 R_SH_IMM_LOW16_PCREL  gdata3
0+88 R_SH_IMM_MEDLOW16_PCREL  gdata4
0+94 R_SH_IMM_LOW16_PCREL  gdata6\+0x0+10
0+98 R_SH_IMM_LOW16_PCREL  gdata7\+0x0+c
0+9c R_SH_IMM_MEDLOW16_PCREL  gdata8\+0x0+4
0+a8 R_SH_IMM_LOW16_PCREL  gothertext2
0+ac R_SH_IMM_LOW16_PCREL  gothertext3
0+b0 R_SH_IMM_MEDLOW16_PCREL  gothertext4
0+bc R_SH_IMM_LOW16_PCREL  gothertext6\+0x0+10
0+c0 R_SH_IMM_LOW16_PCREL  gothertext7\+0x0+c
0+c4 R_SH_IMM_MEDLOW16_PCREL  gothertext8\+0x0+4
0+00 R_SH_IMM_MEDLOW16_PCREL  \.data\+0x0+4
0+04 R_SH_IMM_LOW16_PCREL  \.data\+0x0+8
0+14 R_SH_IMM_MEDLOW16_PCREL  \.data\+0x0+1c
0+18 R_SH_IMM_LOW16_PCREL  \.data\+0x0+20
0+28 R_SH_IMM_MEDLOW16_PCREL  \.othertext\+0x0+5
0+2c R_SH_IMM_LOW16_PCREL  \.othertext\+0x0+9
0+3c R_SH_IMM_MEDLOW16_PCREL  \.othertext\+0x0+1d
0+40 R_SH_IMM_LOW16_PCREL  \.othertext\+0x0+21
0+50 R_SH_IMM_MEDLOW16_PCREL  extern1
0+54 R_SH_IMM_LOW16_PCREL  extern1\+0x0+4
0+64 R_SH_IMM_MEDLOW16_PCREL  extern5\+0x0+8
0+68 R_SH_IMM_LOW16_PCREL  extern5\+0x0+c
0+78 R_SH_IMM_MEDLOW16_PCREL  gdata1
0+7c R_SH_IMM_LOW16_PCREL  gdata1\+0x0+4
0+8c R_SH_IMM_MEDLOW16_PCREL  gdata5\+0x0+8
0+90 R_SH_IMM_LOW16_PCREL  gdata5\+0x0+c
0+a0 R_SH_IMM_MEDLOW16_PCREL  gothertext1
0+a4 R_SH_IMM_LOW16_PCREL  gothertext1\+0x0+4
0+b4 R_SH_IMM_MEDLOW16_PCREL  gothertext5\+0x0+8
0+b8 R_SH_IMM_LOW16_PCREL  gothertext5\+0x0+c

Contents of section \.text:
 0000 cc0000a0 c80000a0 cc0000a0 cc0000a0  .*
 0010 cc0000a0 cc0000a0 c80000a0 cc0000a0  .*
 0020 cc0000a0 cc0000a0 cc0000a0 c80000a0  .*
 0030 cc0000a0 cc0000a0 cc0000a0 cc0000a0  .*
 0040 c80000a0 cc0000a0 cc0000a0 cc0000a0  .*
 0050 cc0000a0 c80000a0 cc0000a0 cc0000a0  .*
 0060 cc0000a0 cc0000a0 c80000a0 cc0000a0  .*
 0070 cc0000a0 cc0000a0 cc0000a0 c80000a0  .*
 0080 cc0000a0 cc0000a0 cc0000a0 cc0000a0  .*
 0090 c80000a0 cc0000a0 cc0000a0 cc0000a0  .*
 00a0 cc0000a0 c80000a0 cc0000a0 cc0000a0  .*
 00b0 cc0000a0 cc0000a0 c80000a0 cc0000a0  .*
 00c0 cc0000a0 cc0000a0                    .*
Contents of section \.data:
 0000 00000000 00000000 00000000 00000000  .*
 0010 00000000 00000000 00000000 00000000  .*
 0020 00000000 00000000 00000000 00000000  .*
 0030 00000000 00000000 00000000 00000000  .*
 0040 00000000                             .*
Contents of section \.othertext:
 0000 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0010 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0020 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0030 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0040 6ff0fff0                             .*
