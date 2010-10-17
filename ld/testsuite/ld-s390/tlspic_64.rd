#source: tlspic1.s
#source: tlspic2.s
#as: -m64 -Aesame
#ld: -shared -melf64_s390
#readelf: -WSsrl
#target: s390x-*-*

There are 18 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] .hash +.*
  \[ 2\] .dynsym +.*
  \[ 3\] .dynstr +.*
  \[ 4\] .rela.dyn +.*
  \[ 5\] .rela.plt +.*
  \[ 6\] .plt +.*
  \[ 7\] .text +PROGBITS +0+790 0+790 0+270 00 +AX +0 +0 +4
  \[ 8\] .data +.*
  \[ 9\] .tdata +PROGBITS +0+1a00 0+a00 0+60 00 WAT +0 +0 +1
  \[10\] .tbss +NOBITS +0+1a60 0+a60 0+20 00 WAT +0 +0 +1
  \[11\] .dynamic +DYNAMIC +0+1a60 0+a60 0+130 10 +WA +3 +0 +8
  \[12\] .got +PROGBITS +0+1b90 0+b90 0+b0 08 +WA +0 +0 +8
  \[13\] .sbss +.*
  \[14\] .bss +.*
  \[15\] .shstrtab +.*
  \[16\] .symtab +.*
  \[17\] .strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is DYN \(Shared object file\)
Entry point 0x790
There are 4 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  LOAD +0x0+ 0x0+ 0x0+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x1000
  LOAD +0x0+a00 0x0+1a00 0x0+1a00 0x0+240 0x0+240 RW +0x1000
  DYNAMIC +0x0+a60 0x0+1a60 0x0+1a60 0x0+130 0x0+130 RW +0x8
  TLS +0x0+a00 0x0+1a00 0x0+1a00 0x0+60 0x0+80 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 +.hash .dynsym .dynstr .rela.dyn .rela.plt .plt .text *
   01 +.tdata .tbss .dynamic .got *
   02 +.tbss .dynamic *
   03 +.tdata .tbss *

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 14 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-z]+  0+36 R_390_TLS_DTPMOD +0+
[0-9a-z]+  0+38 R_390_TLS_TPOFF +0+24
[0-9a-z]+  0+38 R_390_TLS_TPOFF +0+30
[0-9a-z]+  0+36 R_390_TLS_DTPMOD +0+
[0-9a-z]+  0+36 R_390_TLS_DTPMOD +0+
[0-9a-z]+  0+38 R_390_TLS_TPOFF +0+64
[0-9a-z]+  0+38 R_390_TLS_TPOFF +0+50
[0-9a-z]+  0+38 R_390_TLS_TPOFF +0+70
[0-9a-z]+  0+36 R_390_TLS_DTPMOD +0+
[0-9a-z]+  0+38 R_390_TLS_TPOFF +0+44
[0-9a-z]+  0+130+38 R_390_TLS_TPOFF +0+10 sg5 \+ 0
[0-9a-z]+  0+150+36 R_390_TLS_DTPMOD +0+ sg1 \+ 0
[0-9a-z]+  0+150+37 R_390_TLS_DTPOFF +0+ sg1 \+ 0
[0-9a-z]+  0+180+38 R_390_TLS_TPOFF +0+4 sg2 \+ 0

Relocation section '.rela.plt' at offset 0x738 contains 1 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
0+1ba8  0+140+b R_390_JMP_SLOT +0+ __tls_get_offset \+ 0

Symbol table '.dynsym' contains 30 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND 
 +1: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +1 
 +2: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +2 
 +3: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +3 
 +4: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +4 
 +5: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +5 
 +6: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +6 
 +7: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +7 
 +8: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +8 
 +9: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +9 
 +10: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +10 
 +11: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +11 
 +12: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +12 
 +13: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +13 
 +14: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +14 
 +15: 0+1c +0 TLS +GLOBAL DEFAULT +9 sg8
 +16: [0-9a-z]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +17: 0+8 +0 TLS +GLOBAL DEFAULT +9 sg3
 +18: 0+c +0 TLS +GLOBAL DEFAULT +9 sg4
 +19: 0+10 +0 TLS +GLOBAL DEFAULT +9 sg5
 +20: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_offset
 +21: 0+ +0 TLS +GLOBAL DEFAULT +9 sg1
 +22: [0-9a-z]+ +0 FUNC +GLOBAL DEFAULT +7 fn1
 +23: [0-9a-z]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +24: 0+4 +0 TLS +GLOBAL DEFAULT +9 sg2
 +25: 0+14 +0 TLS +GLOBAL DEFAULT +9 sg6
 +26: 0+18 +0 TLS +GLOBAL DEFAULT +9 sg7
 +27: [0-9a-z]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +28: [0-9a-z]+ +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +29: 0+1c40 +0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 57 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND 
 +1: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +1 
 +2: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +2 
 +3: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +3 
 +4: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +4 
 +5: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +5 
 +6: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +6 
 +7: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +7 
 +8: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +8 
 +9: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +9 
 +10: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +10 
 +11: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +11 
 +12: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +12 
 +13: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +13 
 +14: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +14 
 +15: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +15 
 +16: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +16 
 +17: [0-9a-z]+ +0 SECTION LOCAL  DEFAULT +17 
 +18: 0+20 +0 TLS +LOCAL  DEFAULT +9 sl1
 +19: 0+24 +0 TLS +LOCAL  DEFAULT +9 sl2
 +20: 0+28 +0 TLS +LOCAL  DEFAULT +9 sl3
 +21: 0+2c +0 TLS +LOCAL  DEFAULT +9 sl4
 +22: 0+30 +0 TLS +LOCAL  DEFAULT +9 sl5
 +23: 0+34 +0 TLS +LOCAL  DEFAULT +9 sl6
 +24: 0+38 +0 TLS +LOCAL  DEFAULT +9 sl7
 +25: 0+3c +0 TLS +LOCAL  DEFAULT +9 sl8
 +26: 0+60 +0 TLS +LOCAL  HIDDEN +10 sH1
 +27: 0+48 +0 TLS +LOCAL  HIDDEN +9 sh3
 +28: 0+64 +0 TLS +LOCAL  HIDDEN +10 sH2
 +29: 0+78 +0 TLS +LOCAL  HIDDEN +10 sH7
 +30: 0+58 +0 TLS +LOCAL  HIDDEN +9 sh7
 +31: 0+5c +0 TLS +LOCAL  HIDDEN +9 sh8
 +32: 0+6c +0 TLS +LOCAL  HIDDEN +10 sH4
 +33: 0+4c +0 TLS +LOCAL  HIDDEN +9 sh4
 +34: 0+68 +0 TLS +LOCAL  HIDDEN +10 sH3
 +35: 0+50 +0 TLS +LOCAL  HIDDEN +9 sh5
 +36: 0+70 +0 TLS +LOCAL  HIDDEN +10 sH5
 +37: 0+74 +0 TLS +LOCAL  HIDDEN +10 sH6
 +38: 0+7c +0 TLS +LOCAL  HIDDEN +10 sH8
 +39: 0+40 +0 TLS +LOCAL  HIDDEN +9 sh1
 +40: 0+44 +0 TLS +LOCAL  HIDDEN +9 sh2
 +41: 0+54 +0 TLS +LOCAL  HIDDEN +9 sh6
 +42: 0+1c +0 TLS +GLOBAL DEFAULT +9 sg8
 +43: [0-9a-z]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +44: 0+8 +0 TLS +GLOBAL DEFAULT +9 sg3
 +45: 0+c +0 TLS +GLOBAL DEFAULT +9 sg4
 +46: 0+10 +0 TLS +GLOBAL DEFAULT +9 sg5
 +47: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_offset
 +48: 0+ +0 TLS +GLOBAL DEFAULT +9 sg1
 +49: [0-9a-z]+ +0 FUNC +GLOBAL DEFAULT +7 fn1
 +50: [0-9a-z]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +51: 0+4 +0 TLS +GLOBAL DEFAULT +9 sg2
 +52: 0+14 +0 TLS +GLOBAL DEFAULT +9 sg6
 +53: 0+18 +0 TLS +GLOBAL DEFAULT +9 sg7
 +54: [0-9a-z]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +55: [0-9a-z]+ +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +56: [0-9a-z]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
