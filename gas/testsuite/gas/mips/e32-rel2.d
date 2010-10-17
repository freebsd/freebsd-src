#objdump: -sr -j .text
#name: MIPS ELF reloc 2 (32-bit)
#source: elf-rel2.s

# Test the GPREL and LITERAL generation.
# FIXME: really this should check that the contents of .sdata, .lit4,
# and .lit8 are correct too.

.*:     file format elf.*mips

RELOCATION RECORDS FOR \[\.text\]:
OFFSET [ ]+ TYPE              VALUE 
0+0000000 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+0000004 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+0000008 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+000000c R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+0000010 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+0000014 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+0000018 R_MIPS_LITERAL    \.lit4\+0x0+0004000
0+000001c R_MIPS_LITERAL    \.lit4\+0x0+0004000
0+0000020 R_MIPS_LITERAL    \.lit4\+0x0+0004000
0+0000024 R_MIPS_GPREL16    \.sdata\+0x0+0004000
0+0000028 R_MIPS_GPREL16    \.sdata\+0x0+0004000
0+000002c R_MIPS_GPREL16    \.sdata\+0x0+0004000


Contents of section \.text:
 0000 c783c000 c782c004 c783c008 c782c00c  .*
 0010 c783c010 c782c014 c782c000 c782c004  .*
 0020 c782c008 8f82c000 8f82c004 8f82c008  .*
