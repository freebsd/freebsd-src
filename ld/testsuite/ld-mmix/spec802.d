#source: bspec802.s
#source: start.s
#ld: -m mmo
#objdump: -sh

# See spec801.d.
# #2: non-quote LOP in name.

.*:     file format mmo

Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 \.text         0+4  0+  0+  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD, CODE
  1 \.data         0+4  2000000000000000  2000000000000000  0+  2\*\*2
                  CONTENTS, ALLOC, LOAD
  2 \.MMIX\.spec_data\.80 0+4  0+  0+  0+  2\*\*2
                  CONTENTS
Contents of section \.text:
 0000 e3fd0001                             .*
Contents of section \.data:
 2000000000000000 00112233                             .*
Contents of section \.MMIX\.spec_data\.80:
 0000 00000004                             .*
