#objdump: -rs -j .data -j "\$DATA\$"
#name: .equ redefinitions (2)

.*: .*

RELOCATION RECORDS FOR .*
.*
0+00.*(here|\.data|\$DATA\$)
0+08.*xtrn
0+10.*(sym|(\.data|\$DATA\$)(\+0x0+10)?)
#...
Contents of section (\.data|\$DATA\$):
 0000 00000000 11111111 00000000 22222222[ 	]+................[ 	]*
 0010 [01]00000[01]0 .*
#pass
