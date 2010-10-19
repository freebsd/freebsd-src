# objdump: -xsr

.*:     file format elf64-mmix
.*
architecture: mmix, flags 0x00000011:
HAS_RELOC, HAS_SYMS
start address 0x0000000000000000

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         00000004  0000000000000000  0000000000000000  00000040  2\*\*2
                  CONTENTS, ALLOC, LOAD, RELOC, READONLY, CODE
  1 \.data         00000000  0000000000000000  0000000000000000  00000044  2\*\*0
                  CONTENTS, ALLOC, LOAD, DATA
  2 \.bss          00000000  0000000000000000  0000000000000000  00000044  2\*\*0
                  ALLOC
SYMBOL TABLE:
0+ l    d  \.text	0+ (|\.text)
0+ l    d  \.data	0+ (|\.data)
0+ l    d  \.bss	0+ (|\.bss)
0+90 l       \*REG\*	0+ reghere
0+2d l       \*ABS\*	0+ consthere
0+         \*UND\*	0+ extreg


RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0+ R_MMIX_LOCAL      extreg
0+ R_MMIX_LOCAL      reghere
0+ R_MMIX_LOCAL      consthere
0+ R_MMIX_LOCAL      \*ABS\*\+0x0000000000000064
0+ R_MMIX_LOCAL      \*ABS\*\+0x00000000000000c8


Contents of section \.text:
 0000 fd000000                             .*

