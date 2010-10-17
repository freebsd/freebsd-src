#as: --abi=64
#objdump: -sr
#source: rel-5.s
#name: MOVI: PC-relative reloc within .text, 64-bit ABI.

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+5c R_SH_IMM_LOW16_PCREL  gstart6\+0x0+18
0+60 R_SH_IMM_MEDLOW16_PCREL  gstart7\+0x0+20
0+1c R_SH_IMM_HI16_PCREL  gstart2\+0x0+8
0+20 R_SH_IMM_MEDHI16_PCREL  gstart2\+0x0+c
0+24 R_SH_IMM_MEDLOW16_PCREL  gstart2\+0x0+10
0+28 R_SH_IMM_LOW16_PCREL  gstart2\+0x0+14
0+2c R_SH_IMM_HI16_PCREL  gstart3\+0x0+3
0+30 R_SH_IMM_MEDHI16_PCREL  gstart3\+0x0+7
0+34 R_SH_IMM_MEDLOW16_PCREL  gstart3\+0x0+b
0+38 R_SH_IMM_LOW16_PCREL  gstart3\+0x0+f
0+3c R_SH_IMM_HI16_PCREL  gstart4\+0x0+8
0+40 R_SH_IMM_MEDHI16_PCREL  gstart4\+0x0+c
0+44 R_SH_IMM_MEDLOW16_PCREL  gstart4\+0x0+10
0+48 R_SH_IMM_LOW16_PCREL  gstart4\+0x0+14
0+4c R_SH_IMM_HI16_PCREL  gstart5\+0x0+b
0+50 R_SH_IMM_MEDHI16_PCREL  gstart5\+0x0+f
0+54 R_SH_IMM_MEDLOW16_PCREL  gstart5\+0x0+13
0+58 R_SH_IMM_LOW16_PCREL  gstart5\+0x0+17

Contents of section \.text:
 0000 6ff0fff0 cc01a5e0 cc0191e0 cc01a1e0  .*
 0010 cc01ade0 cc01e280 cc000320 cc0001e0  .*
 0020 c80001e0 c80001e0 c80001e0 cc0001e0  .*
 0030 c80001e0 c80001e0 c80001e0 cc0001e0  .*
 0040 c80001e0 c80001e0 c80001e0 cc0001e0  .*
 0050 c80001e0 c80001e0 c80001e0 cc000280  .*
 0060 cc000320 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0070 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0080 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0090 6ff0fff0                             .*

