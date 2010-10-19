#source: start.s
#source: bpo-1.s
#source: bpo-2.s
#source: bpo-5.s
#source: bpo-6.s
#as: -linker-allocated-gregs
#ld: -m elf64mmix -T$srcdir/$subdir/bpo64addr.ld
#objdump: -st

.*:     file format elf64-mmix

SYMBOL TABLE:
0+100 l    d  \.text	0+ (|\.text)
4000000000001060 l    d  \.text\.away	0+ (|\.text\.away)
0+7e0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
4000000000001064 l       \.text\.away	0+ x
0+100 g       \.text	0+ x
4000000000001060 g       \.text\.away	0+ Main
0+104 g       \.text	0+ x2
4000000000001060 g       \.text\.away	0+ _start
4000000000001068 g       \.text\.away	0+ y

Contents of section \.text:
 0100 232dfc00 232dfd00                    .*
Contents of section \.text\.away:
 4000000000001060 e3fd0001 232afe1e 2321fe00           .*
Contents of section \.MMIX\.reg_contents:
 07e0 00000000 00001168 00000000 0000a514  .*
 07f0 40000000 00001070                    .*
