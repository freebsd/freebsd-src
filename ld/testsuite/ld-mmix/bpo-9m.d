#source: start3.s
#source: bpo-6.s
#source: bpo-5.s
#as: -linker-allocated-gregs
#ld: -m mmo --gc-sections
#objdump: -st

# Check that GC does not mess up things when no BPO:s are collected.
# Note that mmo doesn't support GC at the moment; it's a nop.

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+4 g       \.text x
0+ g       \.text x2

Contents of section \.text:
 0000 232dfe00 232dfd00 00000000 0000002d  .*
 0010 00000000 0000002a                    .*
Contents of section \.MMIX\.reg_contents:
 07e8 00000000 0000106c 00000000 0000a410  .*
