#objdump: -sr -j .text
#name: MIPS ELF reloc
#as: -32

# Test the HI16/LO16 generation.

.*:     file format elf.*mips

RELOCATION RECORDS FOR \[\.text\]:
OFFSET [ ]+ TYPE              VALUE 
0+0000000 R_MIPS_HI16       \.text
0+0000018 R_MIPS_LO16       \.text
0+000000c R_MIPS_HI16       \.text
0+000001c R_MIPS_LO16       \.text
0+0000008 R_MIPS_HI16       \.text
0+0000020 R_MIPS_LO16       \.text
0+0000004 R_MIPS_HI16       \.text
0+0000024 R_MIPS_LO16       \.text
0+0000014 R_MIPS_HI16       \.text
0+0000028 R_MIPS_LO16       \.text
0+0000010 R_MIPS_HI16       \.text
0+000002c R_MIPS_LO16       \.text
0+0000030 R_MIPS_HI16       \.text
0+0000048 R_MIPS_LO16       \.text
0+0000034 R_MIPS_HI16       \.text
0+000004c R_MIPS_LO16       \.text
0+0000038 R_MIPS_HI16       \.text
0+0000050 R_MIPS_LO16       \.text
0+000003c R_MIPS_HI16       \.text
0+0000054 R_MIPS_LO16       \.text
0+0000044 R_MIPS_HI16       \.text
0+0000058 R_MIPS_LO16       \.text
0+0000040 R_MIPS_HI16       \.text
0+000005c R_MIPS_LO16       \.text
0+0000060 R_MIPS_HI16       \.text
0+0000078 R_MIPS_LO16       \.text
0+0000064 R_MIPS_HI16       \.text
0+000007c R_MIPS_LO16       \.text
0+0000068 R_MIPS_HI16       \.text
0+0000080 R_MIPS_LO16       \.text
0+000006c R_MIPS_HI16       \.text
0+0000084 R_MIPS_LO16       \.text
0+0000074 R_MIPS_HI16       \.text
0+0000088 R_MIPS_LO16       \.text
0+0000070 R_MIPS_HI16       \.text
0+000008c R_MIPS_LO16       \.text


Contents of section \.text:
 0000 3c010000 3c010000 3c010001 3c010001  .*
 0010 3c010000 3c010001 20210018 2021001c  .*
 0020 20210018 2021001c 20218018 2021fffc  .*
 0030 3c010001 3c010001 3c010002 3c010002  .*
 0040 3c010001 3c010001 2021bffe 2021c002  .*
 0050 2021bffe 2021c002 20213ffe 2021bffa  .*
 0060 3c010001 3c010001 3c010002 3c010002  .*
 0070 3c010001 3c010001 2021bffe 2021c002  .*
 0080 2021bffe 2021c002 20213ffe 2021bffa  .*
#pass
