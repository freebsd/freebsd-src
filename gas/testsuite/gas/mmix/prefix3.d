#objdump: -str

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+8 l       \.text	0+ someplace
0+ l       \.text	0+ bc:h
0+8 l       \.MMIX\.reg_contents	0+ a1
0+ l       \.MMIX\.reg_contents	0+ ba2
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+4 g     F \.text	0+ Main


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+2 R_MMIX_REG        \.MMIX\.reg_contents\+0x0+8
0+6 R_MMIX_REG        \.MMIX\.reg_contents


RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+8 R_MMIX_64         \.text\+0x0+8


Contents of section \.text:
 0000 81ff0000 81fe0000                    .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 0008aa52 00000000 00000000  .*
