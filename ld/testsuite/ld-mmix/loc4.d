#source: loc1.s
#source: data1.s
#source: start.s
#ld: -m elf64mmix
#objdump: -str

# Two text files and one data.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+1000 l    d  \.text	0+ (|\.text)
2000000000000000 l    d  \.data	0+ (|\.data)
0+ l    d  \*ABS\*	0+ (|\.shstrtab)
0+ l    d  \*ABS\*	0+ (|\.symtab)
0+ l    d  \*ABS\*	0+ (|\.strtab)
2000000000000000 l       \.data	0+ xx
0+1004 g     F \.text	0+ Main
2000000000000000 g       \*ABS\*	0+ __\.MMIX\.start\.\.data
0+1004 g       \.text	0+ _start
0+1000 g       \.text	0+ loc1
0+1000 g       \*ABS\*	0+ __\.MMIX\.start\.\.text
2000000000000008 g       \*ABS\*	0+ __bss_start
2000000000000008 g       \*ABS\*	0+ _edata
2000000000000008 g       \*ABS\*	0+ _end
0+1004 g       \.text	0+ _start\.

Contents of section \.text:
 1000 fd030303 e3fd0001                    .*
Contents of section \.data:
 2000000000000000 00000000 00001030                    .*
