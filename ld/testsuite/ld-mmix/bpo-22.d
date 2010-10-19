#source: start.s
#source: bpo-1.s
#as: -linker-allocated-gregs
#ld: -m mmo --oformat elf64-mmix
#objdump: -st

# This weird combination of format and emulation options caused hiccups in
# the reloc accounting machinery.

.*:     file format elf64-mmix

SYMBOL TABLE:
0000000000000000 l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
0+4 l       \.text	0+ x
0+ g       \.text	0+ Main
0+ g       \.text	0+ _start

Contents of section \.text:
 0000 e3fd0001 232afe00                    .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 0000002e                    .*
