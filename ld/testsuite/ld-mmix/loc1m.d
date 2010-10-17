#source: loc1.s
#ld: -m mmo -e loc1
#objdump: -str

# err: two locs.

.*:     file format mmo

SYMBOL TABLE:
0+1000 g       \.text Main
0+1000 g       \.text loc1

Contents of section \.text:
 1000 fd030303                             .*
