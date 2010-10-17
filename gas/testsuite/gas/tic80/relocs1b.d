#objdump: -r
#source: relocs1.s
#name: TIc80 simple relocs, global/local funcs & branches (relocs)

.*: +file format .*tic80.*

RELOCATION RECORDS FOR \[.text\]:
OFFSET   TYPE              VALUE 
00000010 32                _xfunc
00000034 32                .text
0000007c 32                .text
0000008c 32                _xfunc
