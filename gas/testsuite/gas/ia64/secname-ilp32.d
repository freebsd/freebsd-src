#readelf: -S
#name: ia64 section name (ilp32)
#as: -milp32
#source: secname.s

There are 8 section headers, starting at offset 0x7c:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] .text             PROGBITS        00000000 000040 000000 00  AX  0   0 16
  \[ 2\] .data             PROGBITS        00000000 000040 000000 00  WA  0   0  1
  \[ 3\] .bss              NOBITS          00000000 000040 000000 00  WA  0   0  1
  \[ 4\] .foo              PROGBITS        00000000 000040 000008 00  WA  0   0  8
  \[ 5\] .shstrtab         STRTAB          00000000 000048 000031 00      0   0  1
  \[ 6\] .symtab           SYMTAB          00000000 0001bc 000050 10      7   5  4
  \[ 7\] .strtab           STRTAB          00000000 00020c 000001 00      0   0  1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)
