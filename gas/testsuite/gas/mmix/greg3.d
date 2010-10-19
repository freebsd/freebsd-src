# objdump: -rst

# Check that we emit the right relocations for greg operands.

.*:     file format elf64-mmix

SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+ l    d  \.MMIX\.reg_contents	0+ (|\.MMIX\.reg_contents)
0+ g       \.MMIX\.reg_contents	0+ areg
0+c g     F \.text	0+ Main

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+2 R_MMIX_REG        \.MMIX\.reg_contents
0+7 R_MMIX_REG_OR_BYTE  \.MMIX\.reg_contents
0+a R_MMIX_REG        \.MMIX\.reg_contents

RELOCATION RECORDS FOR \[\.MMIX\.reg_contents\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_64         \.text\+0x0+10

Contents of section \.text:
 0000 8f030010 8e030700 8f050004 fd000001  .*
 0010 fd000002 fd000003 fd000004 fd000005  .*
 0020 fd000006                             .*
Contents of section \.MMIX\.reg_contents:
 0000 00000000 00000000                    .*
