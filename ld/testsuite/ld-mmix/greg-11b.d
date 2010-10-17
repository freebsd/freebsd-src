#source: greg-1.s
#source: gregldo1.s
#source: gregget2.s
#source: a.s
#source: greg-3.s
#source: start.s
#source: greg-2.s
#as: -x
#ld: -m mmo
#objdump: -str

# Have two used gregs and one unused, mmo; display contents to visualize
# mmo bug with register contents.

.*:     file format mmo

SYMBOL TABLE:
0+20 g       \.text Main
0+fe g       \*REG\* b
0+20 g       \.text _start
0+fc g       \*REG\* areg
0+fd g       \*REG\* c
0+1c g       \.text a


Contents of section \.text:
 0+ 8c0c20fc 8d7bfc22 8dfcea38 e3fe001c  .*
 0+10 e6fe0000 e5fe0000 e4fe0000 e3fd0004  .*
 0+20 e3fd0001                             .*
Contents of section \.MMIX\.reg_contents:
 07e0 00007048 860f3a38 00000000 00000042  .*
 07f0 007acf50 505a30a2                    .*
