#readelf: -S
#name: ia64 unwind section

There are 9 section headers, starting at offset 0xa0:

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
  \[ 4\] \.IA_64\.unwind_inf PROGBITS         0000000000000000  00000040
       0000000000000008  0000000000000000   A       0     0     8
  \[ 5\] \.IA_64\.unwind     IA_64_UNWIND     0000000000000000  00000048
       0000000000000008  0000000000000000  AL       1     1     8
  \[ 6\] \.shstrtab         STRTAB           0000000000000000  00000050
       000000000000004d  0000000000000000           0     0     1
  \[ 7\] \.symtab           SYMTAB           0000000000000000  000002e0
       0000000000000090  0000000000000018           8     6     8
  \[ 8\] \.strtab           STRTAB           0000000000000000  00000370
       0000000000000001  0000000000000000           0     0     1
Key to Flags:
  W \(write\), A \(alloc\), X \(execute\), M \(merge\), S \(strings\)
  I \(info\), L \(link order\), G \(group\), x \(unknown\)
  O \(extra OS processing required\) o \(OS specific\), p \(processor specific\)
