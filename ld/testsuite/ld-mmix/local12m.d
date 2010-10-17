#source: local1.s
#source: ext1-254.s
#source: start.s
#ld: -m mmo
#objdump: -shr

# Check that 254 is local when we don't have any registers.

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+8  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
Contents of section \.text:
 0000 fd030201 e3fd0001                    .*
