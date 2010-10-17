#readelf: -Ssrx1 -x2
There are 7 section headers, starting at offset 0x88:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  \[ 1\] \.text             PROGBITS         0000000000000000  00000040
       0000000000000018  0000000000000000  AX       0     0     4
  \[ 2\] \.data             PROGBITS         0000000000000000  00000058
       0000000000000004  0000000000000000  WA       0     0     4
  \[ 3\] \.bss              NOBITS           0000000000000000  0000005c
       0000000000000000  0000000000000000  WA       0     0     1
  \[ 4\] \.shstrtab         STRTAB           0000000000000000  0000005c
       000000000000002c  0000000000000000           0     0     1
  \[ 5\] \.symtab           SYMTAB           0000000000000000  00000248
       00000000000000c0  0000000000000018           6     5     8
  \[ 6\] \.strtab           STRTAB           0000000000000000  00000308
       000000000000003b  0000000000000000           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

There are no relocations in this file\.

Symbol table '\.symtab' contains 8 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0+     0 SECTION LOCAL  DEFAULT    1 
     2: 0+     0 SECTION LOCAL  DEFAULT    2 
     3: 0+     0 SECTION LOCAL  DEFAULT    3 
     4: 2000000000000000     0 NOTYPE  LOCAL  DEFAULT  ABS Data_Segment
     5: 0+14     0 FUNC    GLOBAL DEFAULT    1 Main
     6: 0+     0 NOTYPE  GLOBAL DEFAULT  ABS __\.MMIX\.start\.\.text
     7: 2000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS __\.MMIX\.start\.\.data

Hex dump of section '\.text':
  0x00000000 fd001807 00000000 00000000 00000000 .*
  0x00000010 fd00038f fd090101                   .*

Hex dump of section '\.data':
  0x00000000 00000100                            .*
