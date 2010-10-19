#source: op-0-1.s
#as: -no-expand
#objdump: -srt

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l       \*ABS\*	0+ zero0
0+ l       \*ABS\*	0+ zero1
0+ l       \*ABS\*	0+ zero2
0+ g     F \.text	0+ Main


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_ADDR27     \*ABS\*
0+4 R_MMIX_ADDR19     \*ABS\*
0+8 R_MMIX_ADDR19     \*ABS\*


Contents of section \.text:
 0000 f0000000 f4070000 f2080000           .*

