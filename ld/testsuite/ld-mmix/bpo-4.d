#source: start.s
#source: greg-1.s
#source: bpo-3.s
#source: bpo-1.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#objdump: -st

# Three GREGs: one explicit, two linker-allocated.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7e0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+8 l       \.text	0+ x
0+ g       \.text	0+ _start
0+fe g       \*REG\*	0+ areg
#...

Contents of section \.text:
 0000 e3fd0001 8f79fd00 232afc00           .*
Contents of section \.MMIX\.reg_contents:
 07e0 00000000 00000032 00000000 00000133  .*
 07f0 00007048 860f3a38                    .*
