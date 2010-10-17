#source: tlspic1.s
#source: tlspic2.s
#as: --32
#ld: -shared -melf_i386
#readelf: -Ssrl
#target: i?86-*-*

There are [0-9]+ section headers, starting at offset 0x.*:

Section Headers:
  \[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] \.hash +.*
  \[ 2\] \.dynsym +.*
  \[ 3\] \.dynstr +.*
  \[ 4\] \.rel.dyn +.*
  \[ 5\] \.rel.plt +.*
  \[ 6\] \.plt +.*
  \[ 7\] \.text +.*
  \[ 8\] \.data +.*
  \[ 9\] .tdata +PROGBITS +[0-9a-f]+ [0-9a-f]+ 000060 00 WAT  0   0  1
  \[10\] .tbss +NOBITS +[0-9aa-f]+ [0-9a-f]+ 000020 00 WAT  0   0  1
  \[11\] \.dynamic +.*
  \[12\] \.got +.*
  \[13\] \.bss +.*
  \[14\] \.shstrtab +.*
  \[15\] \.symtab +.*
  \[16\] \.strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is DYN \(Shared object file\)
Entry point 0x[0-9a-f]+
There are [0-9]+ program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  LOAD.*
  LOAD.*
  DYNAMIC.*
  TLS +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x0+60 0x0+80 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 +.hash .dynsym .dynstr .rel.dyn .rel.plt .plt .text *
   01 +.tdata .tbss .dynamic .got *
   02 +.tbss .dynamic *
   03 +.tdata .tbss *

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 26 entries:
 Offset +Info +Type +Sym.Value +Sym. Name
[0-9a-f]+ +0+23 R_386_TLS_DTPMOD3
[0-9a-f]+ +0+25 R_386_TLS_TPOFF32
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+25 R_386_TLS_TPOFF32
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+23 R_386_TLS_DTPMOD3
[0-9a-f]+ +0+23 R_386_TLS_DTPMOD3
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+25 R_386_TLS_TPOFF32
[0-9a-f]+ +0+25 R_386_TLS_TPOFF32
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+25 R_386_TLS_TPOFF32
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+0e R_386_TLS_TPOFF *
[0-9a-f]+ +0+23 R_386_TLS_DTPMOD3
[0-9a-f]+ +0+25 R_386_TLS_TPOFF32
[0-9a-f]+ +0+100e R_386_TLS_TPOFF   0+8   sg3
[0-9a-f]+ +0+1125 R_386_TLS_TPOFF32 0+c   sg4
[0-9a-f]+ +0+110e R_386_TLS_TPOFF   0+c   sg4
[0-9a-f]+ +0+120e R_386_TLS_TPOFF   0+10   sg5
[0-9a-f]+ +0+1323 R_386_TLS_DTPMOD3 0+   sg1
[0-9a-f]+ +0+1324 R_386_TLS_DTPOFF3 0+   sg1
[0-9a-f]+ +0+1625 R_386_TLS_TPOFF32 0+4   sg2

Relocation section '.rel.plt' at offset 0x480 contains 1 entries:
 Offset     Info    Type            Sym.Value  Sym. Name
[0-9a-f]+  0+1c07 R_386_JUMP_SLOT   0+   ___tls_get_addr

Symbol table '.dynsym' contains 29 entries:
 +Num: + Value  Size Type + Bind +Vis +Ndx Name
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
 +14: 0+1c +0 TLS +GLOBAL DEFAULT +9 sg8
 +15: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +16: 0+8 +0 TLS +GLOBAL DEFAULT +9 sg3
 +17: 0+c +0 TLS +GLOBAL DEFAULT +9 sg4
 +18: 0+10 +0 TLS +GLOBAL DEFAULT +9 sg5
 +19: 0+ +0 TLS +GLOBAL DEFAULT +9 sg1
 +20: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +7 fn1
 +21: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +22: 0+4 +0 TLS +GLOBAL DEFAULT +9 sg2
 +23: 0+14 +0 TLS +GLOBAL DEFAULT +9 sg6
 +24: 0+18 +0 TLS +GLOBAL DEFAULT +9 sg7
 +25: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +26: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +27: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +28: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND ___tls_get_addr

Symbol table '.symtab' contains 56 entries:
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
 +14: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +14 *
 +15: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +15 *
 +16: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +16 *
 +17: 0+20 +0 TLS +LOCAL  DEFAULT +9 sl1
 +18: 0+24 +0 TLS +LOCAL  DEFAULT +9 sl2
 +19: 0+28 +0 TLS +LOCAL  DEFAULT +9 sl3
 +20: 0+2c +0 TLS +LOCAL  DEFAULT +9 sl4
 +21: 0+30 +0 TLS +LOCAL  DEFAULT +9 sl5
 +22: 0+34 +0 TLS +LOCAL  DEFAULT +9 sl6
 +23: 0+38 +0 TLS +LOCAL  DEFAULT +9 sl7
 +24: 0+3c +0 TLS +LOCAL  DEFAULT +9 sl8
 +25: 0+60 +0 TLS +LOCAL  HIDDEN +10 sH1
 +26: 0+48 +0 TLS +LOCAL  HIDDEN +9 sh3
 +27: 0+64 +0 TLS +LOCAL  HIDDEN +10 sH2
 +28: 0+78 +0 TLS +LOCAL  HIDDEN +10 sH7
 +29: 0+58 +0 TLS +LOCAL  HIDDEN +9 sh7
 +30: 0+5c +0 TLS +LOCAL  HIDDEN +9 sh8
 +31: 0+6c +0 TLS +LOCAL  HIDDEN +10 sH4
 +32: 0+4c +0 TLS +LOCAL  HIDDEN +9 sh4
 +33: 0+68 +0 TLS +LOCAL  HIDDEN +10 sH3
 +34: 0+50 +0 TLS +LOCAL  HIDDEN +9 sh5
 +35: 0+70 +0 TLS +LOCAL  HIDDEN +10 sH5
 +36: 0+74 +0 TLS +LOCAL  HIDDEN +10 sH6
 +37: 0+7c +0 TLS +LOCAL  HIDDEN +10 sH8
 +38: 0+40 +0 TLS +LOCAL  HIDDEN +9 sh1
 +39: 0+44 +0 TLS +LOCAL  HIDDEN +9 sh2
 +40: 0+54 +0 TLS +LOCAL  HIDDEN +9 sh6
 +41: 0+1c +0 TLS +GLOBAL DEFAULT +9 sg8
 +42: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +43: 0+8 +0 TLS +GLOBAL DEFAULT +9 sg3
 +44: 0+c +0 TLS +GLOBAL DEFAULT +9 sg4
 +45: 0+10 +0 TLS +GLOBAL DEFAULT +9 sg5
 +46: 0+ +0 TLS +GLOBAL DEFAULT +9 sg1
 +47: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +7 fn1
 +48: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +49: 0+4 +0 TLS +GLOBAL DEFAULT +9 sg2
 +50: 0+14 +0 TLS +GLOBAL DEFAULT +9 sg6
 +51: 0+18 +0 TLS +GLOBAL DEFAULT +9 sg7
 +52: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +53: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +54: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +55: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND ___tls_get_addr
