#source: bspec1.s
#source: bspec2.s
#source: bspec1.s
#source: start.s
#source: ext1.s
#ld: -m elf64mmix
#readelf: -Ssr -x1 -x2 -x3

There are 7 section headers, starting at offset 0xd0:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0+  0+
       0+  0+           0     0     0
  \[ 1\] \.text             PROGBITS         0+  0+78
       0+4  0+  AX       0     0     4
  \[ 2\] \.MMIX\.spec_data\.2 PROGBITS         0+  0+7c
       0+8  0+           0     0     4
  \[ 3\] \.MMIX\.spec_data\.3 PROGBITS         0+  0+84
       0+4  0+           0     0     4
  \[ 4\] \.shstrtab         STRTAB           0+  0+88
       0+45  0+           0     0     1
  \[ 5\] \.symtab           SYMTAB           0+  0+290
       0+150  0+18           6     7     8
  \[ 6\] \.strtab           STRTAB           0+  0+3e0
       0+32  0+           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

There are no relocations in this file\.

Symbol table '\.symtab' contains 14 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0+     0 SECTION LOCAL  DEFAULT    1 
     2: 0+     0 SECTION LOCAL  DEFAULT    2 
     3: 0+     0 SECTION LOCAL  DEFAULT    3 
     4: 0+     0 SECTION LOCAL  DEFAULT    4 
     5: 0+     0 SECTION LOCAL  DEFAULT    5 
     6: 0+     0 SECTION LOCAL  DEFAULT    6 
     7: 0+     0 FUNC    GLOBAL DEFAULT    1 Main
     8: 0+fc     0 NOTYPE  GLOBAL DEFAULT  ABS ext1
     9: 0+     0 NOTYPE  GLOBAL DEFAULT    1 _start
#...

Hex dump of section '\.text':
  0x0+ e3fd0001                            .*

Hex dump of section '\.MMIX\.spec_data\.2':
  0x0+ 0000002a 0000002a                   .*

Hex dump of section '\.MMIX\.spec_data\.3':
  0x0+ 000000fc                            .*
