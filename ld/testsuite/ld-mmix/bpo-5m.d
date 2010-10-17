#source: start.s
#source: bpo-1.s
#source: bpo-3.s
#source: bpo-2.s
#as: -linker-allocated-gregs
#ld: -m mmo
#objdump: -st

# Three linker-allocated GREGs: one eliminated.

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start
0+c g       \.text y

Contents of section \.text:
 0000 e3fd0001 232afd1a 8f79fe00 2321fd00  .*
Contents of section \.MMIX\.reg_contents:
 07e8 00000000 00000014 00000000 00000133  .*
