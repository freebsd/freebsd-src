#objdump: -str

# Branches can have base-plus-offset operands too.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+4 l       \.text	0+ x
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+a R_MMIX_REG        \.MMIX\.reg_contents


RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_64         \.text


Contents of section \.text:
 0000 fd000000 fd000001 9f000004           .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000                    .*
