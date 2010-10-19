#objdump: -str

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+10 g       \*ABS\*	0+ __\.MMIX\.start\.\.text


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+2a R_MMIX_REG        \.MMIX\.reg_contents
0+30 R_MMIX_64         \.text\+0x0+28
0+38 R_MMIX_64         \.text\+0x0+40


RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_64         \.text\+0x0+5a


Contents of section \.text:
 0000 05000000 00000000 00000000 00000000  .*
 0010 00000000 00000000 00000000 00000000  .*
 0020 00000000 fd000000 231e0000 00000000  .*
 0030 00000000 00000000 00000000 00000000  .*
 0040 fd000000 002a002b 002b002c           .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000                    .*
