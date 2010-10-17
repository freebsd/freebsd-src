#source: weak1.s
#as: -little
#ld: -e 0x1000 -EL
#objdump: -ts
#target: sh*-*-elf

.*:     file format elf32-sh.*

SYMBOL TABLE:
#...
0+10a0 l       .data	0+ d0
0+1000 l       .text	0+ f
0+10a4  w      .data	0+ w0
#...

Contents of section .text:
 1000 01d11260 0b000900 a4100000 09000900  .*
 1010 09000900 09000900 09000900 09000900  .*
Contents of section .data:
 10a0 01000000 00000000                    .*
#pass
