#source: loct.s
#source: start.s
#ld: -m elf64mmix
#objdump: -str

.*:     file format elf64-mmix

SYMBOL TABLE:
0+1004 l    d  \.text	0+ (|\.text)
0+1004 l       \.text	0+ t
0+100c g       \.text	0+ _start
0+1004 g       \*ABS\*	0+ __\.MMIX\.start\.\.text
2000000000000000 g       \*ABS\*	0+ __bss_start
2000000000000000 g       \*ABS\*	0+ _edata
2000000000000000 g       \*ABS\*	0+ _end
0+100c g       \.text	0+ _start\.

Contents of section \.text:
 1004 fd000000 00001004 e3fd0001           .*
