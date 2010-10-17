#source: start.s
#source: loc1.s
#ld: -m mmo
#objdump: -str

.*:     file format mmo

SYMBOL TABLE:
0+1000 g       \.text Main
0+1000 g       \.text _start
0+1004 g       \.text loc1

Contents of section \.text:
 1000 e3fd0001 fd030303                    .*
