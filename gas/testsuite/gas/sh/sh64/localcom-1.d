#as: --abi=32
#objdump: -srt
#source: localcom-1.s
#name: Datalabel on local comm symbol and equated local comm symbol

.*:     file format .*-sh64.*

SYMBOL TABLE:
0+0 l    d  \.text	0+ (|\.text)
0+0 l    d  \.data	0+ (|\.data)
0+0 l    d  \.bss	0+ (|\.bss)
0+0 l       \.text	0+ start
0+c l     O \.bss	0+4 dd
0+c l     O \.bss	0+4 d
0+4 l     O \.bss	0+4 b
0+0 l     O \.bss	0+4 a
0+8 l     O \.bss	0+4 c


RELOCATION RECORDS FOR \[\.text\]:
OFFSET  *TYPE  *VALUE 
0+10 R_SH_DIR32        \.bss
0+14 R_SH_DIR32        \.bss
0+18 R_SH_DIR32        \.bss


Contents of section \.text:
 0000 00090009 00090009 00090009 00090009  .*
 0010 00000004 00000004 0000000c 12340009  .*

