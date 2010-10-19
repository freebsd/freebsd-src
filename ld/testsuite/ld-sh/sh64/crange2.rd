.*

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] \.init             PROGBITS        00001000 000080 000004 00 AXp  0   0  4
  \[ 2\] \.text             PROGBITS        00001004 000084 00005c 00 AXp  0   0  4
  \[ 3\] \.stack            PROGBITS        00080000 000100 000004 00  WA  0   0  1
  \[ 4\] \.cranges          LOUSER\+1        00000000 000104 000046 00   W  0   0  1
  \[ 5\] \.shstrtab         STRTAB          .*
  \[ 6\] \.symtab           SYMTAB          .*
  \[ 7\] \.strtab           STRTAB          .*
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

There are no relocations in this file\.

Symbol table '\.symtab' contains [0-9]+ entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
.*: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
.*: 00001000     0 SECTION LOCAL  DEFAULT    1 
.*: 00001004     0 SECTION LOCAL  DEFAULT    2 
.*: 00080000     0 SECTION LOCAL  DEFAULT    3 
.*: 00000000     0 SECTION LOCAL  DEFAULT    4 
.*: 00000000     0 SECTION LOCAL  DEFAULT    5 
.*: 00000000     0 SECTION LOCAL  DEFAULT    6 
.*: 00000000     0 SECTION LOCAL  DEFAULT    7 
.*: 00001004     0 NOTYPE  LOCAL  DEFAULT    2 start2
.*: 0000101c     0 NOTYPE  LOCAL  DEFAULT    2 sec1
.*: 0000102c     0 NOTYPE  LOCAL  DEFAULT    2 sec2
.*: 00001040     0 NOTYPE  LOCAL  DEFAULT    2 sec3
.*: 00001048     0 NOTYPE  LOCAL  DEFAULT    2 sec4
.*: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS ___dtors
.*: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.*: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS ___ctors_end
.*: 00001004     0 NOTYPE  GLOBAL DEFAULT    2 diversion2
.*: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS ___ctors
.*: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS _edata
.*: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS _end
.*: 00001000     0 NOTYPE  GLOBAL DEFAULT    1 start
.*: 00080000     0 NOTYPE  GLOBAL DEFAULT    3 _stack
.*: 000010e0     0 NOTYPE  GLOBAL DEFAULT  ABS ___dtors_end

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
