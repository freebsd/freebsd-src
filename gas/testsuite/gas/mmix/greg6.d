#objdump: -str

# GAS must know that .data and expressions around #20 << 56 can be
# equivalent for GREGs; like greg5 but the other way round.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l       \.text	0+ t
2000000000000004 l       \*ABS\*	0+ x
2000000000000000 l       \*ABS\*	0+ Data_Segment
0+ l       \.data	0+ y
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ g     F \.text	0+ Main
2000000000000008 g       \*ABS\*	0+ __\.MMIX\.start\.\.data


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+2 R_MMIX_REG        \.MMIX\.reg_contents


RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_64         \.data


Contents of section \.text:
 0000 232c0054                             .*
Contents of section \.data:
 0000 00000000 00000021                    .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000                    .*
