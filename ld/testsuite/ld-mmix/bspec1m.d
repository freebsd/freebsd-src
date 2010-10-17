#source: bspec1.s
#source: start.s
#ld: -m mmo
#objdump: -str

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start

Contents of section \.text:
 0+ e3fd0001                             .*
Contents of section \.MMIX\.spec_data\.2:
 0000 0000002a                             .*
