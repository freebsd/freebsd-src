#source: start.s
#ld: -m mmo -u undefd
#objdump: -x

.*:     file format mmo
.*
architecture: mmix, flags 0x0+10:
HAS_SYMS
start address 0x0+

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+4  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
SYMBOL TABLE:
0+ g       \.text Main
0+ g       \*UND\* undefd
0+ g       \.text _start
