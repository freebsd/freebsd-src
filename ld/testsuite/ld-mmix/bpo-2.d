#source: start.s
#source: greg-1.s
#source: bpo-1.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix
#objdump: -st

# Just a simple linker-allocated GREG plus one explicit GREG.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7e8 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+4 l       \.text	0+ x
0+ g       \.text	0+ _start
0+fe g       \*REG\*	0+ areg
#...

Contents of section \.text:
 0000 e3fd0001 232afd00                    .*
Contents of section \.MMIX\.reg_contents:
 07e8 00000000 0000002e 00007048 860f3a38  .*
