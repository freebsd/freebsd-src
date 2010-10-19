#objdump: -str

# GAS must know that .text and expressions around 0 can be equivalent for
# GREGs.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l       \.text	0+ t
0+4 l       \*ABS\*	0+ x
0+ l       \.text	0+ y
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+8 g     F \.text	0+ Main
0+8 g       \*ABS\*	0+ __\.MMIX\.start\.\.text


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+a R_MMIX_REG        \.MMIX\.reg_contents

Contents of section \.text:
 0000 00000000 00000021 232c0004           .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000004                    .*
