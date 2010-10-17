#source: dloc1.s
#source: start.s
#ld: -m mmo
#objdump: -str

# Text files and one loc:ed data at offset.

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
2000000000000200 g       \.data dloc1
0+ g       \.text _start

Contents of section \.text:
 0000 e3fd0001                             .*
Contents of section \.data:
 2000000000000200 00000004 00000005 00000006           .*
