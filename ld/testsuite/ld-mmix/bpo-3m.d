#source: start.s
#source: bpo-1.s
#source: bpo-2.s
#as: -linker-allocated-gregs
#ld: -m mmo
#objdump: -st

# Just two BPO relocs merged as one linker-allocated GREG.

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start
0+8 g       \.text y

Contents of section \.text:
 0000 e3fd0001 232afe1e 2321fe00           .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 00000010                    .*
