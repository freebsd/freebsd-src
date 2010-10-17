#readelf: -Ssrx1
There are 7 section headers, starting at offset 0x88:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  \[ 1\] \.text             PROGBITS         0000000000000000  00000040
       0000000000000016  0000000000000000  AX       0     0     4
  \[ 2\] \.data             PROGBITS         0000000000000000  00000056
       0000000000000000  0000000000000000  WA       0     0     1
  \[ 3\] \.bss              NOBITS           0000000000000000  00000056
       0000000000000000  0000000000000000  WA       0     0     1
  \[ 4\] \.shstrtab         STRTAB           0000000000000000  00000056
       000000000000002c  0000000000000000           0     0     1
  \[ 5\] \.symtab           SYMTAB           0000000000000000  00000248
       00000000000000c0  0000000000000018           6     7     8
  \[ 6\] \.strtab           STRTAB           0000000000000000  00000308
       0000000000000018  0000000000000000           0     0     1
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
     4: 0+2a     0 NOTYPE  LOCAL  DEFAULT  ABS number
     5: 0+4     0 NOTYPE  LOCAL  DEFAULT    1 label
     6: 0+c     0 NOTYPE  LOCAL  DEFAULT    1 lab2
     7: 0+     0 FUNC    GLOBAL DEFAULT    1 Main

Hex dump of section '\.text':
  0x00000000 fd2b2a29 73747269 6e670aff 8e007374 .*
  0x00000010 72696e67 320a                       .*
