#source: start.s
#source: greg-1.s
#source: bpo-3.s
#source: bpo-1.s
#as: -linker-allocated-gregs
#ld: -m mmo
#objdump: -st

# Three GREGs: one explicit, two linker-allocated.

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start
0+fe g       \*REG\* areg

Contents of section \.text:
 0000 e3fd0001 8f79fd00 232afc00           .*
Contents of section \.MMIX\.reg_contents:
 07e0 00000000 00000032 00000000 00000133  .*
 07f0 00007048 860f3a38                    .*
