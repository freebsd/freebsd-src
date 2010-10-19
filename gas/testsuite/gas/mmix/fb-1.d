#objdump: -str

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+2 R_MMIX_REG        \.MMIX\.reg_contents


Contents of section \.text:
 0+ dd2d0038                             .*
Contents of section \.MMIX\.reg_contents:
 0+ 00000000 aabbccdd 00000000 00112233  .*
