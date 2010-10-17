#source: start.s
#source: aregm.s
#source: gregldo1.s
#ld: -m mmo
#objdump: -str

.*:     file format mmo

SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start
0+fe g       \*REG\* areg

Contents of section \.text:
 0+ e3fd0001 8f03fe10 8e0307fe 8f05fe04  .*
 0+10 8c0c20fe 8d7bfe22 8dfeea38           .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 00000004                    .*
