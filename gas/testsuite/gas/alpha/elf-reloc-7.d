#objdump: -r
#name: alpha elf-reloc-7

.*:     file format elf64-alpha.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0*0000008 BRADDR            bar


RELOCATION RECORDS FOR \[\.data\]:
OFFSET           TYPE              VALUE 
0*0000004 SREL32            \.data2\+0x0*0000004
0*0000008 SREL32            BAR


RELOCATION RECORDS FOR \[\.text2\]:
OFFSET           TYPE              VALUE 
0*0000004 BRADDR            \.text\+0x0*0000010
0*0000008 BRADDR            bar


