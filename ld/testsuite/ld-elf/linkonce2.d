#source: linkonce1a.s
#source: linkonce1b.s
#ld: -emit-relocs
#objdump: -r

.*:     file format .*

RELOCATION RECORDS FOR \[.debug_frame\]:
OFFSET[ 	]+TYPE[ 	]+VALUE[ 	]*
.*(NONE|unused).*\*ABS\*

#pass
