#readelf: -S
#name: ia64 xdata (ilp32)
#as: -milp32
#source: xdata.s

There are 19 section headers, starting at offset 0x[[:xdigit:]]+:

Section Headers:
  \[Nr\] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  \[ 0\]                   NULL            00000000 000000 000000 00      0   0  0
  \[ 1\] .text             PROGBITS        00000000 [[:xdigit:]]+ 000000 00  AX  0   0 16
  \[ 2\] .data             PROGBITS        00000000 [[:xdigit:]]+ 000000 00  WA  0   0  1
  \[ 3\] .bss              NOBITS          00000000 [[:xdigit:]]+ 000000 00  WA  0   0  1
  \[ 4\] \.xdata1           PROGBITS        00000000 [[:xdigit:]]+ 000001 00   A  0   0  1
  \[ 5\] \.xdata2           PROGBITS        00000000 [[:xdigit:]]+ 000004 00   A  0   0  2
  \[ 6\] ,xdata3           PROGBITS        00000000 [[:xdigit:]]+ 000008 00   A  0   0  4
  \[ 7\] \.xdata,4          PROGBITS        00000000 [[:xdigit:]]+ 000010 00   A  0   0  8
  \[ 8\] "\.xdata5"         PROGBITS        00000000 [[:xdigit:]]+ 000020 00   A  0   0 16
  \[ 9\] \.rela"\.xdata5"    RELA            00000000 [[:xdigit:]]+ 000018 0c     17   8  4
  \[10\] \.xreal\\1          PROGBITS        00000000 [[:xdigit:]]+ 000008 00   A  0   0  4
  \[11\] \.xreal\+2          PROGBITS        00000000 [[:xdigit:]]+ 000010 00   A  0   0  8
  \[12\] \.xreal\(3\)         PROGBITS        00000000 [[:xdigit:]]+ 000014 00   A  0   0 16
  \[13\] \.xreal\[4\]         PROGBITS        00000000 [[:xdigit:]]+ 000020 00   A  0   0 16
  \[14\] \.xstr<1>          PROGBITS        00000000 [[:xdigit:]]+ 000003 00   A  0   0  1
  \[15\] \.xstr\{2\}          PROGBITS        00000000 [[:xdigit:]]+ 000004 00   A  0   0  1
  \[16\] .shstrtab         STRTAB          00000000 [[:xdigit:]]+ [[:xdigit:]]+ 00      0   0  1
  \[17\] .symtab           SYMTAB          00000000 [[:xdigit:]]+ [[:xdigit:]]+ 10     18  15  4
  \[18\] .strtab           STRTAB          00000000 [[:xdigit:]]+ [[:xdigit:]]+ 00      0   0  1
#pass
