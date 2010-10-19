There are 11 section headers, starting at offset 0x128:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] \.text             PROGBITS        00000000 000034 000000 00  AX  0   0  1
  \[ 2\] \.text\.mixed       PROGBITS        00000000 000034 00005c 00 AXp  0   0  4
  \[ 3\] \.data             PROGBITS        00000000 000090 000000 00  WA  0   0  1
  \[ 4\] \.bss              NOBITS          00000000 000090 000000 00  WA  0   0  1
  \[ 5\] \.stack            PROGBITS        00000000 000090 000004 00  WA  0   0  1
  \[ 6\] \.cranges          PROGBITS        00000000 000094 000046 00   W  0   0  1
  \[ 7\] \.rela\.cranges     RELA            00000000 0002e0 000054 0c      9   6  4
  \[ 8\] \.shstrtab         STRTAB          00000000 0000da 00004d 00      0   0  1
  \[ 9\] \.symtab           SYMTAB          00000000 000334 000110 10     10  16  4
  \[10\] \.strtab           STRTAB          00000000 000444 000027 00      0   0  1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

Relocation section '\.rela\.cranges' at offset 0x[0-9a-f]+ contains 7 entries:
.*
0*00000000 +0+0201 R_SH_DIR32 +00000000 +\.text\.mixed +\+ 0
0*0000000a +0+0201 R_SH_DIR32 +00000000 +\.text\.mixed +\+ 0
0*00000014 +0+0201 R_SH_DIR32 +00000000 +\.text\.mixed +\+ 0
0*0000001e +0+0201 R_SH_DIR32 +00000000 +\.text\.mixed +\+ 0
0*00000028 +0+0201 R_SH_DIR32 +00000000 +\.text\.mixed +\+ 0
0*00000032 +0+0201 R_SH_DIR32 +00000000 +\.text\.mixed +\+ 0
0*0000003c +0+0201 R_SH_DIR32 +00000000 +\.text\.mixed +\+ 0

Symbol table '\.symtab' contains 17 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 00000000     0 SECTION LOCAL  DEFAULT    1 
     2: 00000000     0 SECTION LOCAL  DEFAULT    2 
     3: 00000000     0 SECTION LOCAL  DEFAULT    3 
     4: 00000000     0 SECTION LOCAL  DEFAULT    4 
     5: 00000000     0 SECTION LOCAL  DEFAULT    5 
     6: 00000000     0 SECTION LOCAL  DEFAULT    6 
     7: 00000000     0 SECTION LOCAL  DEFAULT    7 
     8: 00000000     0 SECTION LOCAL  DEFAULT    8 
     9: 00000000     0 SECTION LOCAL  DEFAULT    9 
    10: 00000000     0 SECTION LOCAL  DEFAULT   10 
    11: 00000000     0 NOTYPE  LOCAL  DEFAULT    2 start2
    12: 00000018     0 NOTYPE  LOCAL  DEFAULT    2 sec1
    13: 00000028     0 NOTYPE  LOCAL  DEFAULT    2 sec2
    14: 0000003c     0 NOTYPE  LOCAL  DEFAULT    2 sec3
    15: 00000044     0 NOTYPE  LOCAL  DEFAULT    2 sec4
    16: 00000000     0 NOTYPE  GLOBAL DEFAULT    2 diversion2

Hex dump of section '\.text\.mixed':
  0x00000000 6ff0fff0 6ff0fff0 6ff0fff0 0000002a .*
  0x00000010 0000002b 00090009 6ff0fff0 6ff0fff0 .*
  0x00000020 6ff0fff0 6ff0fff0 00000029 0000002b .*
  0x00000030 0000002a 0000002b 0000002a 00090009 .*
  0x00000040 00090000 6ff0fff0 6ff0fff0 6ff0fff0 .*
  0x00000050 6ff0fff0 6ff0fff0 6ff0fff0          .*

Hex dump of section '\.cranges':
  0x00000000 00000000 0000000c 00030000 000c0000 .*
  0x00000010 00080001 00000014 00000004 00020000 .*
  0x00000020 00180000 00100003 00000028 00000014 .*
  0x00000030 00010000 003c0000 00060002 00000044 .*
  0x00000040 00000018 0003                       .*
