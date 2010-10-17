#source: bspec801.s
#source: start.s
#ld: -m mmo
#objdump: -sh

# Check exceptional cases for LOP_SPEC 80, which we parse according to a
# specific format: see documentation and mmo.c
# #1: name length has LOP_QUOTE.

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         00000004  0000000000000000  0000000000000000  00000000  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 \.MMIX\.spec_data\.80 00000004  0000000000000000  0000000000000000  00000000  2\*\*2
                  CONTENTS
Contents of section \.text:
 0000 e3fd0001                             .*
Contents of section \.MMIX\.spec_data\.80:
 0000 98000001                             .*
