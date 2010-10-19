# source: greg2.s
# as: -no-merge-gregs
# objdump: -rst

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l       \.text	0+ D4
0+4 l       \.text	0+ E6
0+ l       \.MMIX\.reg_contents	0+ H9
0+8 l       \.MMIX\.reg_contents	0+ G8
0+18 l       \.MMIX\.reg_contents	0+ F7
0+28 l       \.MMIX\.reg_contents	0+ D5
0+30 l       \.MMIX\.reg_contents	0+ C3
0+38 l       \.MMIX\.reg_contents	0+ B1
0+40 l       \.MMIX\.reg_contents	0+ A0
0+0 l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+c g     F \.text	0+ Main


RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_64         \.text\+0x0+8
0+10 R_MMIX_64         \.text\+0x0+8
0+18 R_MMIX_64         \.text\+0x0+8
0+20 R_MMIX_64         \.text\+0x0+1c
0+30 R_MMIX_64         .text

Contents of section \.text:
 0000 e37b01c8 e3ea1edb fd020304 fd010203  .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000 00000000 000000f7  .*
 0010 00000000 00000000 00000000 00000000  .*
 0020 00000000 00000000 00000000 00000000  .*
 0030 00000000 00000000 00000000 00000001  .*
 0040 00000000 00000000                    .*
