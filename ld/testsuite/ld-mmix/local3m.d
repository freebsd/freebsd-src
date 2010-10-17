#source: greg-4.s
#source: greg-4.s
#source: local1.s
#source: ext1.s
#source: start.s
#ld: -m mmo
#objdump: -str

.*:     file format mmo

SYMBOL TABLE:
0+4 g       \.text Main
0+fc g       \*ABS\* ext1
0+4 g       \.text _start

Contents of section \.text:
 0000 fd030201 e3fd0001                    .*
Contents of section \.MMIX\.reg_contents:
 07e8 00000000 0000004e 00000000 0000004e  .*
