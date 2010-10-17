#source: start.s
#source: sec-6.s
#source: a.s
#as: -x
#ld: -m elf64mmix
#objcopy_linked_file: -O mmo
#objdump: -xs

# A non-loaded section with relocs would have the SEC_RELOC bit set in the
# output if we didn't clear it.  For reference, here's the ELF copied to
# mmo, so we make sure no spurious flags are introduced.

.*:     file format mmo
.*
architecture: mmix, flags 0x0+10:
HAS_SYMS
start address 0x0+

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+8  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 \.debug_frame  0+10  0+  0+  0+  2\*\*2
                  CONTENTS, READONLY, DEBUGGING
SYMBOL TABLE:
0+ g       \.text Main
0+ g       \.text _start
0+8 g       \.text debugb
2000000000000000 g       \*ABS\* __bss_start
2000000000000000 g       \*ABS\* _edata
2000000000000000 g       \*ABS\* _end
0+4 g       \.text a

Contents of section \.text:
 0000 e3fd0001 e3fd0004                    .*
Contents of section \.debug_frame:
 0000 00000000 00000004 00000000 00000008  .*
