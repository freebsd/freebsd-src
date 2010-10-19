#objdump: -sr -j .text
#name: MIPS ELF reloc
#source: elf-rel.s
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
 0000 0000013c 0000013c 0100013c 0100013c  .*
 0010 0000013c 0100013c 18002120 1c002120  .*
 0020 18002120 1c002120 18802120 fcff2120  .*
 0030 0100013c 0100013c 0200013c 0200013c  .*
 0040 0100013c 0100013c febf2120 02c02120  .*
 0050 febf2120 02c02120 fe3f2120 fabf2120  .*
 0060 0100013c 0100013c 0200013c 0200013c  .*
 0070 0100013c 0100013c febf2120 02c02120  .*
 0080 febf2120 02c02120 fe3f2120 fabf2120  .*
#pass
