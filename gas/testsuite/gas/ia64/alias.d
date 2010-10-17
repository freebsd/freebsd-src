#readelf: -Ss
#name: ia64 alias and secalias

There are 8 section headers, starting at offset 0x78:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  \[ 1\] \.text             PROGBITS         0000000000000000  00000040
       0000000000000000  0000000000000000  AX       0     0     16
  \[ 2\] \.data             PROGBITS         0000000000000000  00000040
       0000000000000000  0000000000000000  WA       0     0     1
  \[ 3\] \.bss              NOBITS           0000000000000000  00000040
       0000000000000000  0000000000000000  WA       0     0     1
  \[ 4\] 1234              PROGBITS         0000000000000000  00000040
       0000000000000005  0000000000000000  WA       0     0     1
  \[ 5\] \.shstrtab         STRTAB           0000000000000000  00000045
       0000000000000031  0000000000000000           0     0     1
  \[ 6\] \.symtab           SYMTAB           0000000000000000  00000278
       0000000000000090  0000000000000018           7     6     8
  \[ 7\] \.strtab           STRTAB           0000000000000000  00000308
       0000000000000006  0000000000000000           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

Symbol table '\.symtab' contains 6 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 
     2: 0000000000000000     0 SECTION LOCAL  DEFAULT    2 
     3: 0000000000000000     0 SECTION LOCAL  DEFAULT    3 
     4: 0000000000000000     0 SECTION LOCAL  DEFAULT    4 
     5: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT    4 "@D"
