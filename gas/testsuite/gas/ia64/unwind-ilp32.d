#readelf: -S
#name: ia64 unwind section (ilp32)
#as: -milp32
#source: unwind.s

There are 9 section headers, starting at offset 0xa0:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] .text             PROGBITS        00000000 000040 000000 00  AX  0   0 16
  \[ 2\] .data             PROGBITS        00000000 000040 000000 00  WA  0   0  1
  \[ 3\] .bss              NOBITS          00000000 000040 000000 00  WA  0   0  1
  \[ 4\] .IA_64.unwind_inf PROGBITS        00000000 000040 000008 00   A  0   0  8
  \[ 5\] .IA_64.unwind     IA_64_UNWIND    00000000 000048 000008 00  AL  1   1  8
  \[ 6\] .shstrtab         STRTAB          00000000 000050 00004d 00      0   0  1
  \[ 7\] .symtab           SYMTAB          00000000 000208 000060 10      8   6  4
  \[ 8\] .strtab           STRTAB          00000000 000268 000001 00      0   0  1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)
