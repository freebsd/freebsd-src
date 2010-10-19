#source: start.s
#source: bpo-1.s
#source: bpo-3.s
#source: bpo-2.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#objdump: -st

# Three linker-allocated GREGs: one eliminated.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7e8 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+4 l       \.text	0+ x
0+ g       \.text	0+ _start
#...
0+c g       \.text	0+ y
#...

Contents of section \.text:
 0000 e3fd0001 232afd1a 8f79fe00 2321fd00  .*
Contents of section \.MMIX\.reg_contents:
 07e8 00000000 00000014 00000000 00000133  .*
