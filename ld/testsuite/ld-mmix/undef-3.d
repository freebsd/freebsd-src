#source: start.s
#ld: -u undefd -m elf64mmix
#readelf: -S -s

There are 5 section headers, starting at offset 0xa0:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0+  0+
       0+  0+           0     0     0
  \[ 1\] \.text             PROGBITS         0+  0+78
       0+4  0+  AX       0     0     4
  \[ 2\] \.shstrtab         STRTAB           0+  0+7c
       0+21  0+           0     0     1
  \[ 3\] \.symtab           SYMTAB           0+  0+1e0
       0+c0  0+18           4     2     8
  \[ 4\] \.strtab           STRTAB           0+  0+2a0
       0+2f  0+           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

Symbol table '\.symtab' contains 8 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0+     0 SECTION LOCAL  DEFAULT    1 
     2: 0+     0 NOTYPE  GLOBAL DEFAULT  UND undefd
     3: 0+     0 NOTYPE  GLOBAL DEFAULT    1 _start
     4: 2000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
     5: 2000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS _edata
     6: 2000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS _end
     7: 0+     0 NOTYPE  GLOBAL DEFAULT    1 _start\.
