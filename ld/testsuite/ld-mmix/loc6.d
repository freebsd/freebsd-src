#source: dloc1.s
#source: start.s
#ld: -m elf64mmix
#objdump: -str

# Text files and one loc:ed data at offset.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
2000000000000200 l    d  \.data	0+ (|\.data)
2000000000000200 g       \.data	0+ dloc1
2000000000000200 g       \*ABS\*	0+ __\.MMIX\.start\.\.data
0+ g       \.text	0+ _start
200000000000020c g       \*ABS\*	0+ __bss_start
200000000000020c g       \*ABS\*	0+ _edata
2000000000000210 g       \*ABS\*	0+ _end
0+ g       \.text	0+ _start\.

Contents of section \.text:
 0000 e3fd0001                             .*
Contents of section \.data:
 2000000000000200 00000004 00000005 00000006           .*
