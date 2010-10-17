#readelf: -Ss
#name: ia64 alias and secalias (ilp32)
#as: -milp32
#source: alias.s

There are 8 section headers, starting at offset 0x78:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] .text             PROGBITS        00000000 000040 000000 00  AX  0   0 16
  \[ 2\] .data             PROGBITS        00000000 000040 000000 00  WA  0   0  1
  \[ 3\] .bss              NOBITS          00000000 000040 000000 00  WA  0   0  1
  \[ 4\] 1234              PROGBITS        00000000 000040 000005 00  WA  0   0  1
  \[ 5\] .shstrtab         STRTAB          00000000 000045 000031 00      0   0  1
  \[ 6\] .symtab           SYMTAB          00000000 0001b8 000060 10      7   6  4
  \[ 7\] .strtab           STRTAB          00000000 000218 000006 00      0   0  1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

Symbol table '.symtab' contains 6 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 00000000     0 SECTION LOCAL  DEFAULT    1 
     2: 00000000     0 SECTION LOCAL  DEFAULT    2 
     3: 00000000     0 SECTION LOCAL  DEFAULT    3 
     4: 00000000     0 SECTION LOCAL  DEFAULT    4 
     5: 00000000     0 NOTYPE  LOCAL  DEFAULT    4 "@D"
