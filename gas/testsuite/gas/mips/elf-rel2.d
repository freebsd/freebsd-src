#objdump: -sr -j .text
#name: MIPS ELF reloc 2
#as: -mabi=o64

# Test the GPREL and LITERAL generation.
# FIXME: really this should check that the contents of .sdata, .lit4,
# and .lit8 are correct too.

.*:     file format elf.*mips

RELOCATION RECORDS FOR \[\.text\]:
OFFSET [ ]+ TYPE              VALUE 
0+0000000 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+0000004 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+0000008 R_MIPS_LITERAL    \.lit8\+0x0+0004000
0+000000c R_MIPS_LITERAL    \.lit4\+0x0+0004000
0+0000010 R_MIPS_LITERAL    \.lit4\+0x0+0004000
0+0000014 R_MIPS_LITERAL    \.lit4\+0x0+0004000
0+0000018 R_MIPS_GPREL16    \.sdata\+0x0+0004000
0+000001c R_MIPS_GPREL16    \.sdata\+0x0+0004000
0+0000020 R_MIPS_GPREL16    \.sdata\+0x0+0004000


Contents of section \.text:
 0000 d782c000 d782c008 d782c010 c782c000  .*
 0010 c782c004 c782c008 8f82c000 8f82c004  .*
 0020 8f82c008 .*
