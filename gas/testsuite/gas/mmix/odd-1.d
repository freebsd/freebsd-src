#objdump: -str

# A few odd mmixal compatibility cases.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l       \.MMIX\.reg_contents	0+ small
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ g     F \.text	0+ Main


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+7 R_MMIX_REG_OR_BYTE  \.MMIX\.reg_contents
0+f R_MMIX_REG        \.MMIX\.reg_contents
0+15 R_MMIX_REG        \.MMIX\.reg_contents
0+19 R_MMIX_REG        \.MMIX\.reg_contents

Contents of section \.text:
 0000 f9000000 ff016400 fb0000ff fb000000  .*
 0010 00000001 33000408 c1000200 0004022a  .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000abc                    .*
