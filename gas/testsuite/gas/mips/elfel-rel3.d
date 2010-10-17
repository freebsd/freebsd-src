#objdump: -sr -j .data
#name: MIPS ELF reloc 3
#source: elf-rel3.s
#as: -32

.*:     file format elf.*mips

RELOCATION RECORDS FOR \[\.data\]:
OFFSET [ ]+ TYPE              VALUE 
0+0000004 R_MIPS_32         b
0+0000008 R_MIPS_32         .data


Contents of section .data:
 0000 12121212 04000000 00000000 00000000  ................
