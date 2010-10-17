#source: start.s
#source: greg-1.s
#source: bpo-1.s
#as: -linker-allocated-gregs
#ld: -m mmo
#objdump: -st

# Just a simple linker-allocated GREG plus one explicit GREG.

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start
0+fe g       \*REG\* areg

Contents of section \.text:
 0000 e3fd0001 232afd00                    .*
Contents of section \.MMIX\.reg_contents:
 07e8 00000000 0000002e 00007048 860f3a38  .*

