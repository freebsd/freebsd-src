#source: start.s
#source: bpo-7.s
#source: areg-t.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#objdump: -st

# A BPO against an external symbol.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+ g       \.text	0+ _start
0+8 g       \.text	0+ areg
#...

Contents of section \.text:
 0000 e3fd0001 234dfe00 fd040810           .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 00000003                    .*
