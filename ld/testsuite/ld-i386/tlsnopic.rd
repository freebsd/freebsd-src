#source: tlsnopic1.s
#source: tlsnopic2.s
#as: --32
#ld: -shared -melf_i386
#readelf: -Ssrl
#target: i?86-*-*

There are 14 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] \.hash +.*
  \[ 2\] \.dynsym +.*
  \[ 3\] \.dynstr +.*
  \[ 4\] \.rel.dyn +.*
  \[ 5\] \.text +PROGBITS +0+1000 .*
  \[ 6\] \.data +.*
  \[ 7\] .tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ 000024 00 WAT  0   0  1
  \[ 8\] \.dynamic +DYNAMIC +0+2000 .*
  \[ 9\] \.got +PROGBITS +0+2080 .*
  \[10\] \.bss +.*
  \[11\] \.shstrtab +.*
  \[12\] \.symtab +.*
  \[13\] \.strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  LOAD.*
  LOAD.*
  DYNAMIC.*
  TLS +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x0+ 0x0+24 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 +.hash .dynsym .dynstr .rel.dyn .text *
   01 +.tbss .dynamic .got *
   02 +.tbss .dynamic *
   03 +.tbss *

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 20 entries:
 Offset +Info +Type +Sym.Value +Sym. Name
0+100d  0+8 R_386_RELATIVE +
0+1017  0+8 R_386_RELATIVE +
0+102e  0+8 R_386_RELATIVE +
0+1038  0+8 R_386_RELATIVE +
0+104f  0+8 R_386_RELATIVE +
0+1059  0+8 R_386_RELATIVE +
0+1067  0+c25 R_386_TLS_TPOFF32 0+   sg3
0+107c  0+25 R_386_TLS_TPOFF32
0+10a4  0+e R_386_TLS_TPOFF +
0+10c4  0+e R_386_TLS_TPOFF +
0+10d9  0+e R_386_TLS_TPOFF +
0+10e4  0+e R_386_TLS_TPOFF +
0+208c  0+e R_386_TLS_TPOFF +
0+2090  0+e R_386_TLS_TPOFF +
0+209c  0+e R_386_TLS_TPOFF +
0+20a0  0+e R_386_TLS_TPOFF +
0+109b  0+d0e R_386_TLS_TPOFF   0+   sg4
0+10ce  0+f0e R_386_TLS_TPOFF   0+   sg5
0+2094  0+100e R_386_TLS_TPOFF   0+   sg1
0+2098  0+120e R_386_TLS_TPOFF   0+   sg2


Symbol table '.dynsym' contains 22 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +1: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +1 *
 +2: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +2 *
 +3: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +3 *
 +4: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +4 *
 +5: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +5 *
 +6: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +6 *
 +7: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 *
 +8: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +8 *
 +9: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +9 *
 +10: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +10 *
 +11: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +12: 0+ +0 TLS +GLOBAL DEFAULT  UND sg3
 +13: 0+ +0 TLS +GLOBAL DEFAULT  UND sg4
 +14: 0+1000 +0 FUNC +GLOBAL DEFAULT +5 fn3
 +15: 0+ +0 TLS +GLOBAL DEFAULT  UND sg5
 +16: 0+ +0 TLS +GLOBAL DEFAULT  UND sg1
 +17: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +18: 0+ +0 TLS +GLOBAL DEFAULT  UND sg2
 +19: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +20: 0+2080 +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +21: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 34 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +1: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +1 *
 +2: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +2 *
 +3: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +3 *
 +4: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +4 *
 +5: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +5 *
 +6: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +6 *
 +7: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 *
 +8: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +8 *
 +9: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +9 *
 +10: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +10 *
 +11: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +11 *
 +12: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +12 *
 +13: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +13 *
 +14: 0+00 +0 TLS +LOCAL  DEFAULT +7 bl1
 +15: 0+04 +0 TLS +LOCAL  DEFAULT +7 bl2
 +16: 0+08 +0 TLS +LOCAL  DEFAULT +7 bl3
 +17: 0+0c +0 TLS +LOCAL  DEFAULT +7 bl4
 +18: 0+10 +0 TLS +LOCAL  DEFAULT +7 bl5
 +19: 0+1c +0 TLS +LOCAL  HIDDEN +7 sh3
 +20: 0+20 +0 TLS +LOCAL  HIDDEN +7 sh4
 +21: 0+14 +0 TLS +LOCAL  HIDDEN +7 sh1
 +22: 0+18 +0 TLS +LOCAL  HIDDEN +7 sh2
 +23: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +24: 0+ +0 TLS +GLOBAL DEFAULT  UND sg3
 +25: 0+ +0 TLS +GLOBAL DEFAULT  UND sg4
 +26: 0+1000 +0 FUNC +GLOBAL DEFAULT +5 fn3
 +27: 0+ +0 TLS +GLOBAL DEFAULT  UND sg5
 +28: 0+ +0 TLS +GLOBAL DEFAULT  UND sg1
 +29: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +30: 0+ +0 TLS +GLOBAL DEFAULT  UND sg2
 +31: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +32: 0+2080 +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +33: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
