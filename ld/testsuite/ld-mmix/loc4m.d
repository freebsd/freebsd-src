#source: loc1.s
#source: data1.s
#source: start.s
#ld: -m mmo
#objdump: -str

.*:     file format mmo

SYMBOL TABLE:
0+1004 g       \.text Main
0+1004 g       \.text _start
0+1000 g       \.text loc1

Contents of section \.text:
 1000 fd030303 e3fd0001                    .*
Contents of section \.data:
 2000000000000004 00001030                             .*
