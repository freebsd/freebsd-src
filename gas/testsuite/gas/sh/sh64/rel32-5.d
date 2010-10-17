#as: --abi=32
#objdump: -sr
#source: rel-5.s
#name: MOVI: PC-relative reloc within .text, 32-bit ABI.

.*:     file format .*-sh64.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET  *TYPE  *VALUE 
0+3c R_SH_IMM_LOW16_PCREL  gstart6\+0x0+18
0+40 R_SH_IMM_MEDLOW16_PCREL  gstart7\+0x0+20
0+1c R_SH_IMM_MEDLOW16_PCREL  gstart2\+0x0+8
0+20 R_SH_IMM_LOW16_PCREL  gstart2\+0x0+c
0+24 R_SH_IMM_MEDLOW16_PCREL  gstart3\+0x0+3
0+28 R_SH_IMM_LOW16_PCREL  gstart3\+0x0+7
0+2c R_SH_IMM_MEDLOW16_PCREL  gstart4\+0x0+8
0+30 R_SH_IMM_LOW16_PCREL  gstart4\+0x0+c
0+34 R_SH_IMM_MEDLOW16_PCREL  gstart5\+0x0+b
0+38 R_SH_IMM_LOW16_PCREL  gstart5\+0x0+f

Contents of section \.text:
 0000 6ff0fff0 cc0125e0 cc0111e0 cc0121e0  .*
 0010 cc012de0 cc016280 cc000320 cc0001e0  .*
 0020 c80001e0 cc0001e0 c80001e0 cc0001e0  .*
 0030 c80001e0 cc0001e0 c80001e0 cc000280  .*
 0040 cc000320 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0050 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0060 6ff0fff0 6ff0fff0 6ff0fff0 6ff0fff0  .*
 0070 6ff0fff0                             .*

