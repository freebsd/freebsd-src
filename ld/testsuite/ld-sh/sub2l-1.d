#source: sub2l.s
#as: -little
#ld: -EL -e 0x1000
#objdump: -st
#target: sh*-*-elf

.*/dump:     file format elf32-sh.*

SYMBOL TABLE:
#...
0+1000 l       .text	00000000 f
0+1002 l       .text	00000000 f2
0+1028 l       .text	00000000 L
0+1020 g       .text	00000000 ff
#...

Contents of section \.text:
 1000 0b000900 09000900 09000900 09000900  .*
 1010 09000900 09000900 09000900 09000900  .*
 1020 09000900 09000900 0b000900 d8ffffff  .*
 1030 daffffff 02100000 28100000 24100000  .*
Contents of section \..*:
#pass
