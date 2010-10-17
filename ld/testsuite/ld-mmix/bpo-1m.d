#source: start.s
#source: bpo-1.s
#as: -linker-allocated-gregs
#ld: -m mmo
#objdump: -st

# Just a simple linker-allocated GREG with no explicit GREGs.

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start

Contents of section \.text:
 0000 e3fd0001 232afe00                    .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 0000002e                    .*
