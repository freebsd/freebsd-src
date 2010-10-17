#source: local1.s
#source: ext1-254.s
#source: start.s
#ld: -m elf64mmix
#objdump: -shr

# Check that 254 is local when we don't have any registers.

.*:     file format elf64-mmix

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+8  0+  0+  0+b0  2\*\*2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 \.data         0+  2000000000000000  [0-9]+  0+b8  2\*\*0
                  CONTENTS, ALLOC, LOAD, DATA
  2 \.sbss         0+  2000000000000000  [0-9]+  0+b8  2\*\*0
                  CONTENTS
  3 \.bss          0+  2000000000000000  [0-9]+  0+b8  2\*\*0
                  ALLOC
Contents of section \.text:
 0000 fd030201 e3fd0001                    .*
