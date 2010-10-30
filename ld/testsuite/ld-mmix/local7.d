#source: greg-4.s
#source: greg-4.s
#source: local1.s
#source: local2.s
#source: ext1.s
#source: start.s
#ld: -m elf64mmix
#readelf:  -Ssx1 -x2

# Like local1, but ext1 is here a constant, not a global register and two
# local-register checks.

There are 6 section headers, starting at offset 0xc8:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0+  0+
       0+  0+           0     0     0
  \[ 1\] \.text             PROGBITS         0+  0+78
       0+c  0+  AX       0     0     4
  \[ 2\] \.MMIX\.reg_content PROGBITS         0+7e8  0+84
       0+10  0+   W       0     0     1
  \[ 3\] \.shstrtab         STRTAB           0+  0+94
       0+34  0+           0     0     1
  \[ 4\] \.symtab           SYMTAB           0+  0+248
       0+108  0+18           5     5     8
  \[ 5\] \.strtab           STRTAB           0+  0+350
       0+32  0+           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)

Symbol table '\.symtab' contains 11 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0+     0 SECTION LOCAL  DEFAULT    1 
     2: 0+7e8     0 SECTION LOCAL  DEFAULT    2 
     3: 0+fd     0 NOTYPE  LOCAL  DEFAULT PRC\[0xff00\] lsym
     4: 0+fe     0 NOTYPE  LOCAL  DEFAULT PRC\[0xff00\] lsym
     5: 0+fc     0 NOTYPE  GLOBAL DEFAULT  ABS ext1
     6: 0+8     0 NOTYPE  GLOBAL DEFAULT    1 _start
#...

Hex dump of section '\.text':
  0x0+ fd030201 fd020202 e3fd0001          .*

Hex dump of section '\.MMIX\.reg_contents':
  0x0+7e8 00000000 0000004e 00000000 0000004e .*
