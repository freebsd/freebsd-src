#readelf: -Ssr -x1 -x4
There are 9 section headers, starting at offset 0x..:
#...
  \[ 4\] \.MMIX\.spec_data\.2 PROGBITS         0+  0+44
       0+4  0+           0     0     4
  \[ 5\] \.rela\.MMIX\.spec_d RELA             0+  .*
       0+18  0+18           7     4     8
#...
Relocation section '\.rela\.MMIX\.spec_data\.2' at offset 0x... contains 1 entries:
.*
0+  0+500000004 R_MMIX_32 +0+ +forw +\+ 0

Symbol table '\.symtab' contains 6 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0+     0 SECTION LOCAL  DEFAULT    1 
     2: 0+     0 SECTION LOCAL  DEFAULT    2 
     3: 0+     0 SECTION LOCAL  DEFAULT    3 
     4: 0+     0 SECTION LOCAL  DEFAULT    4 
     5: 0+     0 NOTYPE  GLOBAL DEFAULT  UND forw

Hex dump of section '\.text':
  0x0+ fd010203                            .*

Hex dump of section '\.MMIX\.spec_data\.2':
 NOTE: This section has relocations against it, but these have NOT been applied to this dump.
  0x0+ 00000000                            .*
