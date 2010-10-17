There are 13 section headers, starting at offset 0x1f8:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] \.init             PROGBITS        00001000 000100 000004 00 AXp  0   0  4
  \[ 2\] \.text             PROGBITS        00001004 000104 000018 00 AXp  0   0  4
  \[ 3\] \.data             PROGBITS        000010a0 000120 000000 00  WA  0   0  1
  \[ 4\] \.ctors            PROGBITS        000010a0 000184 000000 00   W  0   0  1
  \[ 5\] \.dtors            PROGBITS        000010a0 000184 000000 00   W  0   0  1
  \[ 6\] \.sbss             PROGBITS        000010a0 000184 000000 00   W  0   0  1
  \[ 7\] \.bss              NOBITS          000010a0 000120 000000 00  WA  0   0  1
  \[ 8\] \.stack            PROGBITS        00080000 000180 000004 00  WA  0   0  1
  \[ 9\] \.cranges          LOUSER\+1        00000000 000184 00001e 00   W  0   0  1
  \[10\] \.shstrtab         STRTAB          00000000 0001a2 000056 00      0   0  1
  \[11\] \.symtab           SYMTAB          00000000 000400 000180 10     12   e  4
  \[12\] \.strtab           STRTAB          00000000 000580 000064 00      0   0  1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

There are no relocations in this file\.

Symbol table '\.symtab' contains 24 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 00001000     0 SECTION LOCAL  DEFAULT    1 
     2: 00001004     0 SECTION LOCAL  DEFAULT    2 
     3: 000010a0     0 SECTION LOCAL  DEFAULT    3 
     4: 000010a0     0 SECTION LOCAL  DEFAULT    4 
     5: 000010a0     0 SECTION LOCAL  DEFAULT    5 
     6: 000010a0     0 SECTION LOCAL  DEFAULT    6 
     7: 000010a0     0 SECTION LOCAL  DEFAULT    7 
     8: 00080000     0 SECTION LOCAL  DEFAULT    8 
     9: 00000000     0 SECTION LOCAL  DEFAULT    9 
    10: 00000000     0 SECTION LOCAL  DEFAULT   10 
    11: 00000000     0 SECTION LOCAL  DEFAULT   11 
    12: 00000000     0 SECTION LOCAL  DEFAULT   12 
    13: 00001004     0 NOTYPE  LOCAL  DEFAULT    2 start2
    14: 000010a0     0 NOTYPE  GLOBAL DEFAULT    5 ___dtors
    15: 000010a0     0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
    16: 000010a0     0 NOTYPE  GLOBAL DEFAULT    4 ___ctors_end
    17: 00001004     0 NOTYPE  GLOBAL DEFAULT    2 diversion2
    18: 000010a0     0 NOTYPE  GLOBAL DEFAULT    4 ___ctors
    19: 000010a0     0 NOTYPE  GLOBAL DEFAULT  ABS _edata
    20: 000010a0     0 NOTYPE  GLOBAL DEFAULT  ABS _end
    21: 00001000     0 NOTYPE  GLOBAL DEFAULT    1 start
    22: 00080000     0 NOTYPE  GLOBAL DEFAULT    8 _stack
    23: 000010a0     0 NOTYPE  GLOBAL DEFAULT    5 ___dtors_end

Hex dump of section '\.init':
  0x00001000 6ff0fff0                            .*

Hex dump of section '\.text':
  0x00001004 6ff0fff0 6ff0fff0 6ff0fff0 0000002a .*
  0x00001014 0000002b 00090009                   .*

Hex dump of section '\.cranges':
  0x00000000 00001004 0000000c 00030000 10100000 .*
  0x00000010 00080001 00001018 00000004 0002     .*
