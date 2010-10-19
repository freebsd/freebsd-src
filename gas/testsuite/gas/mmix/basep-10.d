#as: -linker-allocated-gregs
#objdump: -srt

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+4 l       \.text	0+ w4
0+10 l       \.text	0+ w2
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+c  w      \.text	0+ w1
0+8  w      \.text	0+ w3


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+6 R_MMIX_BASE_PLUS_OFFSET  w1
0+a R_MMIX_REG        \.MMIX\.reg_contents
0+e R_MMIX_REG        \.MMIX\.reg_contents\+0x0+8
0+12 R_MMIX_REG        \.MMIX\.reg_contents\+0x0+8

RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_64         w3
0+8 R_MMIX_64         \.text\+0x0+4

Contents of section \.text:
 0000 fd000000 232a0000 232b0000 232c000c  .*
 0010 232d0000                             .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000 00000000 00000000  .*
