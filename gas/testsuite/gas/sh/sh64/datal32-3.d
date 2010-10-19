#as: --abi=32
#objdump: -xsr
#source: datal-3.s
#name: DataLabel local def/use, SHmedia 32-bit ABI

# We should have the st_type field of each symbol displayed too, so we can
# check that STT_DATALABEL is set, but objdump doesn't do that at present,
# and readelf isn't supported as a run_dump_test tool.

.*:     file format .*-sh64.*
.*
architecture: sh5, flags 0x0+11:
HAS_RELOC, HAS_SYMS
start address 0x0+

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 \.text         0+6c  0+  0+  0+34  2\*\*0
                  CONTENTS, ALLOC, LOAD, RELOC, READONLY, CODE
  1 \.data         0+  0+  0+  0+a0  2\*\*0
                  CONTENTS, ALLOC, LOAD, DATA
  2 \.bss          0+  0+  0+  0+a0  2\*\*0
                  ALLOC
  3 \.rodata       0+10  0+  0+  0+a0  2\*\*2
                  CONTENTS, ALLOC, LOAD, RELOC, READONLY, DATA
SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l       \.text	0+ 0x04 start
0+30 l       \.text	0+ 0x04 foo
0+38 l       \.text	0+ 0x04 foo2
0+40 l       \.text	0+ 0x04 foo3
0+ l    d  \.rodata	0+ (|\.rodata)
0+48 l       \.text	0+ 0x04 foo4
0+4 l       \.rodata	0+ myrodata1
0+50 l       \.text	0+ 0x04 foo5
0+8 l       \.rodata	0+ myrodata2
0+c g       \.rodata	0+ myrodata3
0+60 g       \.text	0+ 0x04 foo7
0+60         \*UND\*	0+ foo7
0+64 g       \.text	0+ 0x04 foo8
0+64         \*UND\*	0+ foo8
0+68 g       \.text	0+ 0x04 foo9
0+68         \*UND\*	0+ foo9
0+58 g       \.text	0+ 0x04 foo6
0+58         \*UND\*	0+ foo6

RELOCATION RECORDS FOR \[\.text\]:
OFFSET  *TYPE  *VALUE 
0+10 R_SH_IMM_MEDLOW16  \.text\+0x0+6e
0+24 R_SH_IMM_MEDLOW16  foo9\+0x0+40
0+00 R_SH_IMM_MEDLOW16  \.text\+0x0+30
0+04 R_SH_IMM_LOW16    \.text\+0x0+30
0+08 R_SH_IMM_MEDLOW16  \.text\+0x0+62
0+0c R_SH_IMM_LOW16    \.text\+0x0+62
0+14 R_SH_IMM_MEDLOW16  foo7\+0x0+2a
0+18 R_SH_IMM_LOW16    foo7\+0x0+2a
0+1c R_SH_IMM_MEDLOW16  foo8
0+20 R_SH_IMM_LOW16    foo8
0+28 R_SH_IMM_MEDLOW16  \.rodata\+0x0+4
0+2c R_SH_IMM_LOW16    \.rodata\+0x0+4
0+30 R_SH_IMM_MEDLOW16  \.rodata\+0x0+26
0+34 R_SH_IMM_LOW16    \.rodata\+0x0+26
0+38 R_SH_IMM_MEDLOW16  \.text\+0x0+30
0+3c R_SH_IMM_LOW16    \.text\+0x0+30
0+40 R_SH_IMM_MEDLOW16  \.text\+0x0+40
0+44 R_SH_IMM_LOW16    \.text\+0x0+40
0+48 R_SH_IMM_MEDLOW16  \.text\+0x0+70
0+4c R_SH_IMM_LOW16    \.text\+0x0+70
0+50 R_SH_IMM_MEDLOW16  myrodata3
0+54 R_SH_IMM_LOW16    myrodata3
0+58 R_SH_IMM_MEDLOW16  foo6\+0x0+2a
0+5c R_SH_IMM_LOW16    foo6\+0x0+2a


RELOCATION RECORDS FOR \[\.rodata\]:
OFFSET  *TYPE  *VALUE 
0+ R_SH_DIR32        \.text
0+4 R_SH_DIR32        \.text
0+8 R_SH_DIR32        \.rodata
0+c R_SH_DIR32        \.rodata


Contents of section \.text:
 0000 cc000030 c8000030 cc000030 c8000030  .*
 0010 cc000030 cc0001e0 c80001e0 cc0001e0  .*
 0020 c80001e0 cc000030 cc000380 c8000380  .*
 0030 cc000150 c8000150 cc0000a0 c80000a0  .*
 0040 cc000210 c8000210 cc000080 c8000080  .*
 0050 cc0002c0 c80002c0 cc0001e0 c80001e0  .*
 0060 6ff0fff0 6ff0fff0 6ff0fff0           .*
Contents of section \.rodata:
 0000 00000048 00000088 00000008 00000020  .*
