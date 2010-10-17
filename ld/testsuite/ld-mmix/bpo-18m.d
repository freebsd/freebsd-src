#source: start.s
#source: bpo-1.s
#source: bpo-2.s
#source: bpo-5.s
#source: bpo-6.s
#as: -linker-allocated-gregs
#ld: -m mmo -T$srcdir/$subdir/bpo64addr.ld
#objdump: -st

.*:     file format mmo

SYMBOL TABLE:
4000000000001060 g       \*ABS\* Main
0+100 g       \.text x
0+104 g       \.text x2
4000000000001060 g       \*ABS\* _start
4000000000001068 g       \*ABS\* y

Contents of section \.text:
 0100 232dfc00 232dfd00                    .*
Contents of section \.text\.away:
 4000000000001060 e3fd0001 232afe1e 2321fe00           .*
Contents of section \.MMIX\.reg_contents:
 07e0 00000000 00001168 00000000 0000a514  .*
 07f0 40000000 00001070                    .*
