#as: -no-expand
#readelf: -Ssrx1 -x6
There are 10 section headers, starting at offset 0x...:
#...
  \[ 5\] \.MMIX\.spec_data\.4 PROGBITS         0+  0+c4
       0+  0+           0     0     1
  \[ 6\] \.MMIX\.reg_content PROGBITS         0+  0+c4
       0+8  0+   W       0     0     1
#...
Relocation section '\.rela\.text' at offset 0x... contains 5 entries:
.*
0+34  .* R_MMIX_ADDR19 +0+ +target +\+ 2c
0+46  .* R_MMIX_16 +0+ +target2 +\+ 30
0+48  .* R_MMIX_ADDR27 +0+ +target3 +\+ 38
0+54  .* R_MMIX_ADDR19 +0+ +target3 +\+ 0
0+78  .* R_MMIX_LOCAL +0+30

Symbol table '\.symtab' contains 12 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0+     0 SECTION LOCAL  DEFAULT    1 
     2: 0+     0 SECTION LOCAL  DEFAULT    3 
     3: 0+     0 SECTION LOCAL  DEFAULT    4 
     4: 0+18     0 NOTYPE  LOCAL  DEFAULT  ABS z
     5: 0+80     0 NOTYPE  LOCAL  DEFAULT    1 x
     6: 0+     0 SECTION LOCAL  DEFAULT    5 
     7: 0+     0 SECTION LOCAL  DEFAULT    6 
     8: 0+     0 FUNC    GLOBAL DEFAULT    1 Main
     9: 0+     0 NOTYPE  GLOBAL DEFAULT  UND target
    10: 0+     0 NOTYPE  GLOBAL DEFAULT  UND target2
    11: 0+     0 NOTYPE  GLOBAL DEFAULT  UND target3

Hex dump of section '\.text':
  0x0+ 0000007b 00010017 00010203 01030201 .*
  0x0+10 09050006 09070208 0509000a 050b030c .*
  0x0+20 230f1011 23121300 23141516 34170018 .*
  0x0+30 34191a1b 401c0000 b91d1e1f bf202122 .*
  0x0+40 c1232400 e0250000 f0000000 f8260027 .*
  0x0+50 f9000028 f2290000 fa2a0000 fb00002b .*
  0x0+60 f604002c fe2d0004 00000000 03020104 .*
  0x0+70 0007000c 00000014 00000000 0000001c .*
  0x0+80 fd221538                            .*

Hex dump of section '\.MMIX\.reg_contents':
  0x0+ 00000000 00000033                   .*
