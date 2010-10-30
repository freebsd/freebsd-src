#source: start.s
#source: aregm.s
#source: gregldo1.s
#ld: -m elf64mmix
#objdump: -str

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+7f0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ g     F \.text	0+ Main
0+ g       \.text	0+ _start
0+fe g       \*REG\*	0+ areg
#...

Contents of section \.text:
 0+ e3fd0001 8f03fe10 8e0307fe 8f05fe04  .*
 0+10 8c0c20fe 8d7bfe22 8dfeea38           .*
Contents of section \.MMIX\.reg_contents:
 07f0 00000000 00000004                    .*
