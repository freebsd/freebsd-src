#objdump: -r
#name: alpha elf-reloc-4

.*:     file format elf64-alpha.*

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0*0000000 ELF_LITERAL       a
0*0000004 LITUSE            \.text\+0x0*0000001
0*0000008 LITUSE            \.text\+0x0*0000002
0*000000c ELF_LITERAL       b
0*0000010 LITUSE            \.text\+0x0*0000001
0*0000014 LITUSE            \.text\+0x0*0000002
0*0000018 ELF_LITERAL       c
0*000001c LITUSE            \.text\+0x0*0000001
0*0000020 LITUSE            \.text\+0x0*0000002
0*0000024 LITUSE            \.text\+0x0*0000002
0*000002c LITUSE            \.text\+0x0*0000001
0*0000030 ELF_LITERAL       d
0*0000034 LITUSE            \.text\+0x0*0000001
0*0000038 LITUSE            \.text\+0x0*0000002
0*000003c LITUSE            \.text\+0x0*0000002
0*0000044 LITUSE            \.text\+0x0*0000001


