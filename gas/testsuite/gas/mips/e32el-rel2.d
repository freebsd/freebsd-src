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
 0000 00c082c7 04c083c7 08c082c7 0cc083c7  .*
 0010 10c082c7 14c083c7 00c082c7 04c082c7  .*
 0020 08c082c7 00c0828f 04c0828f 08c0828f  .*
