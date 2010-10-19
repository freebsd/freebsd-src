#objdump: -srt

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l     O \.bss	0+4 comm_symbol3
0+4 l     O \.bss	0+4 comm_symbol4
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+4       O \*COM\*	0+4 comm_symbol1

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+2 R_MMIX_REG        \.MMIX\.reg_contents\+0x0+8
0+6 R_MMIX_REG        \.MMIX\.reg_contents
0+a R_MMIX_REG        \.MMIX\.reg_contents

RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_64         \.bss
0+8 R_MMIX_64         comm_symbol1

Contents of section \.text:
 0000 232a0000 232c0000 232d0004           .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000 00000000 00000000  .*
