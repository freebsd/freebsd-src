#source: bspec1.s
#source: bspec2.s
#source: bspec1.s
#source: start.s
#source: ext1.s
#ld: -m mmo
#objdump: -str

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+fc g       \*ABS\* ext1
0+ g       \.text _start

Contents of section \.text:
 0+ e3fd0001                             .*
Contents of section \.MMIX\.spec_data\.2:
 0000 0000002a 0000002a                    .*
Contents of section \.MMIX\.spec_data\.3:
 0000 000000fc                             .*
