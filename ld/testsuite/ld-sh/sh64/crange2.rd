There are 13 section headers, starting at offset 0x220:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] \.init             PROGBITS        00001000 000100 000004 00 AXp  0   0  4
  \[ 2\] \.text             PROGBITS        00001004 000104 00005c 00 AXp  0   0  4
  \[ 3\] \.data             PROGBITS        000010e0 000160 000000 00  WA  0   0  1
  \[ 4\] \.ctors            PROGBITS        000010e0 000184 000000 00   W  0   0  1
  \[ 5\] \.dtors            PROGBITS        000010e0 000184 000000 00   W  0   0  1
  \[ 6\] \.sbss             PROGBITS        000010e0 000184 000000 00   W  0   0  1
  \[ 7\] \.bss              NOBITS          000010e0 000160 000000 00  WA  0   0  1
  \[ 8\] \.stack            PROGBITS        00080000 000180 000004 00  WA  0   0  1
  \[ 9\] \.cranges          LOUSER\+1        00000000 000184 000046 00   W  0   0  1
  \[10\] \.shstrtab         STRTAB          00000000 0001ca 000056 00      0   0  1
  \[11\] \.symtab           SYMTAB          00000000 000428 0001c0 10     12  12  4
  \[12\] \.strtab           STRTAB          00000000 0005e8 000078 00      0   0  1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

There are no relocations in this file\.

Symbol table '\.symtab' contains 28 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 00001000     0 SECTION LOCAL  DEFAULT    1 
     2: 00001004     0 SECTION LOCAL  DEFAULT    2 
     3: 000010e0     0 SECTION LOCAL  DEFAULT    3 
     4: 000010e0     0 SECTION LOCAL  DEFAULT    4 
     5: 000010e0     0 SECTION LOCAL  DEFAULT    5 
     6: 000010e0     0 SECTION LOCAL  DEFAULT    6 
     7: 000010e0     0 SECTION LOCAL  DEFAULT    7 
     8: 00080000     0 SECTION LOCAL  DEFAULT    8 
     9: 00000000     0 SECTION LOCAL  DEFAULT    9 
    10: 00000000     0 SECTION LOCAL  DEFAULT   10 
    11: 00000000     0 SECTION LOCAL  DEFAULT   11 
    12: 00000000     0 SECTION LOCAL  DEFAULT   12 
    13: 00001004     0 NOTYPE  LOCAL  DEFAULT    2 start2
    14: 0000101c     0 NOTYPE  LOCAL  DEFAULT    2 sec1
    15: 0000102c     0 NOTYPE  LOCAL  DEFAULT    2 sec2
    16: 00001040     0 NOTYPE  LOCAL  DEFAULT    2 sec3
    17: 00001048     0 NOTYPE  LOCAL  DEFAULT    2 sec4
    18: 000010e0     0 NOTYPE  GLOBAL DEFAULT    5 ___dtors
    19: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
    20: 000010e0     0 NOTYPE  GLOBAL DEFAULT    4 ___ctors_end
    21: 00001004     0 NOTYPE  GLOBAL DEFAULT    2 diversion2
    22: 000010e0     0 NOTYPE  GLOBAL DEFAULT    4 ___ctors
    23: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS _edata
    24: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS _end
    25: 00001000     0 NOTYPE  GLOBAL DEFAULT    1 start
    26: 00080000     0 NOTYPE  GLOBAL DEFAULT    8 _stack
    27: 000010e0     0 NOTYPE  GLOBAL DEFAULT    5 ___dtors_end

Hex dump of section '\.text':
  0x00001004 6ff0fff0 6ff0fff0 6ff0fff0 0000002a .*
  0x00001014 0000002b 00090009 6ff0fff0 6ff0fff0 .*
  0x00001024 6ff0fff0 6ff0fff0 00000029 0000002b .*
  0x00001034 0000002a 0000002b 0000002a 00090009 .*
  0x00001044 00090000 6ff0fff0 6ff0fff0 6ff0fff0 .*
  0x00001054 6ff0fff0 6ff0fff0 6ff0fff0          .*

Hex dump of section '\.cranges':
  0x00000000 00001004 0000000c 00030000 10100000 .*
  0x00000010 00080001 00001018 00000004 00020000 .*
  0x00000020 101c0000 00100003 0000102c 00000014 .*
  0x00000030 00010000 10400000 00060002 00001048 .*
  0x00000040 00000018 0003                       .*
