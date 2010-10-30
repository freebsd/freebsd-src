#source: start.s
#source: bpo-1.s
#source: bpo-2.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#objdump: -st

# Just two BPO relocs merged as one linker-allocated GREG.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+4 l       \.text	0+ x
0+ g       \.text	0+ _start
#...
0+8 g       \.text	0+ y
#...

Contents of section \.text:
 0000 e3fd0001 232afe1e 2321fe00           .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 00000010                    .*
