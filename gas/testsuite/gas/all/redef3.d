#objdump: -rsj .data
#name: .equ redefinitions (3)

.*: .*

RELOCATION RECORDS FOR .*
.*
0+00.*(here|\.data)
0+08.*xtrn
0+10.*sym
#...
Contents of section \.data:
 0000 00000000 11111111 00000000 22222222[ 	]+................[ 	]*
 0010 00000000 .*
#pass
