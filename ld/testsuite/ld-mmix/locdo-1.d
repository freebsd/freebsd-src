#source: locdo.s -globalize-symbols
#source: start.s
#ld: -m elf64mmix
#objdump: -str

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
2000000000000008 l    d  \.data	0+ (|\.data)
2000000000000008 g       \*ABS\*	0+ __\.MMIX\.start\.\.data
2000000000000008 g       \.data	0+ od
0+ g       \.text	0+ _start
2000000000000010 g       \*ABS\*	0+ __bss_start
2000000000000000 g       \*ABS\*	0+ Data_Segment
2000000000000010 g       \*ABS\*	0+ _edata
2000000000000010 g       \*ABS\*	0+ _end
0+ g       \.text	0+ _start\.

Contents of section \.text:
 0000 e3fd0001                             .*
Contents of section \.data:
 2000000000000008 20000000 00000008                    .*
