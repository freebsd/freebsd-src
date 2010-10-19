#readelf: -S
#name: x86-64 unwind

There are 8 section headers, starting at offset 0x80:

Section Headers:
  \[Nr\] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  \[ 0\]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  \[ 1\] \.text             PROGBITS         0000000000000000  00000040
       0000000000000000  0000000000000000  AX       0     0     4
  \[ 2\] \.data             PROGBITS         0000000000000000  00000040
       0000000000000000  0000000000000000  WA       0     0     4
  \[ 3\] \.bss              NOBITS           0000000000000000  00000040
       0000000000000000  0000000000000000  WA       0     0     4
  \[ 4\] \.eh_frame         X86_64_UNWIND    0000000000000000  00000040
       0000000000000008  0000000000000000   A       0     0     1
  \[ 5\] \.shstrtab         STRTAB           0000000000000000  00000048
       0000000000000036  0000000000000000           0     0     1
  \[ 6\] \.symtab           SYMTAB           0000000000000000  00000280
       0000000000000078  0000000000000018           7     5     8
  \[ 7\] \.strtab           STRTAB           0000000000000000  000002f8
       0000000000000001  0000000000000000           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)
