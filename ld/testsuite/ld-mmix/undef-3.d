#source: start.s
#ld: -u undefd -m elf64mmix
#readelf: -S -s

There are 8 section headers, starting at offset 0xe8:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0+  0+
       0+  0+           0     0     0
  \[ 1\] \.text             PROGBITS         0+  0+b0
       0+4  0+  AX       0     0     4
  \[ 2\] \.data             PROGBITS         20+  0+b4
       0+  0+  WA       0     0     1
  \[ 3\] \.sbss             PROGBITS         2000000000000000  0+b4
       0+  0+   W       0     0     1
  \[ 4\] \.bss              NOBITS           2000000000000000  0+b4
       0+  0+  WA       0     0     1
  \[ 5\] \.shstrtab         STRTAB           0+  0+b4
       0+32  0+           0     0     1
  \[ 6\] \.symtab           SYMTAB           0+  0+2e8
       0+150  0+18           7     8     8
  \[ 7\] \.strtab           STRTAB           0+  0+438
       0+2f  0+           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

Symbol table '\.symtab' contains 14 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0+     0 SECTION LOCAL  DEFAULT    1 
     2: 2000000000000000     0 SECTION LOCAL  DEFAULT    2 
     3: 2000000000000000     0 SECTION LOCAL  DEFAULT    3 
     4: 2000000000000000     0 SECTION LOCAL  DEFAULT    4 
     5: 0+     0 SECTION LOCAL  DEFAULT    5 
     6: 0+     0 SECTION LOCAL  DEFAULT    6 
     7: 0+     0 SECTION LOCAL  DEFAULT    7 
     8: 0+     0 NOTYPE  GLOBAL DEFAULT  UND undefd
     9: 0+     0 NOTYPE  GLOBAL DEFAULT    1 _start
    10: 2000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
    11: 2000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS _edata
    12: 2000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS _end
    13: 0+     0 NOTYPE  GLOBAL DEFAULT    1 _start\.
