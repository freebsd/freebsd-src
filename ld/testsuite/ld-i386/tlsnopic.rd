#source: tlsnopic1.s
#source: tlsnopic2.s
#as: --32
#ld: -shared -melf_i386
#readelf: -Ssrl
#target: i?86-*-*

There are 13 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] \.hash +.*
  \[ 2\] \.dynsym +.*
  \[ 3\] \.dynstr +.*
  \[ 4\] \.rel.dyn +.*
  \[ 5\] \.text +PROGBITS +0+1000 .*
  \[ 6\] \.tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ 000024 00 WAT  0   0  1
  \[ 7\] \.dynamic +DYNAMIC +0+20f4 .*
  \[ 8\] \.got +PROGBITS +0+2174 .*
  \[ 9\] \.got.plt +PROGBITS +0+218c .*
  \[10\] \.shstrtab +.*
  \[11\] \.symtab +.*
  \[12\] \.strtab +.*
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
   01 +.dynamic .got .got.plt *
   02 +.dynamic *
   03 +.tbss *

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 20 entries:
 Offset +Info +Type +Sym.Value +Sym. Name
[0-9a-f ]+R_386_RELATIVE +
[0-9a-f ]+R_386_RELATIVE +
[0-9a-f ]+R_386_RELATIVE +
[0-9a-f ]+R_386_RELATIVE +
[0-9a-f ]+R_386_RELATIVE +
[0-9a-f ]+R_386_RELATIVE +
[0-9a-f ]+R_386_TLS_TPOFF32 0+   sg3
[0-9a-f ]+R_386_TLS_TPOFF32
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF +
[0-9a-f ]+R_386_TLS_TPOFF   0+   sg4
[0-9a-f ]+R_386_TLS_TPOFF   0+   sg5
[0-9a-f ]+R_386_TLS_TPOFF   0+   sg1
[0-9a-f ]+R_386_TLS_TPOFF   0+   sg2


Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +[0-9]+: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg4
 +[0-9]+: 0+1000 +0 FUNC +GLOBAL DEFAULT +5 fn3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg2
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 30 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +[0-9]+: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +1 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +2 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +3 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +4 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +5 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +6 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +8 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +9 *
 +[0-9]+: 0+00 +0 TLS +LOCAL  DEFAULT +6 bl1
 +[0-9]+: 0+04 +0 TLS +LOCAL  DEFAULT +6 bl2
 +[0-9]+: 0+08 +0 TLS +LOCAL  DEFAULT +6 bl3
 +[0-9]+: 0+0c +0 TLS +LOCAL  DEFAULT +6 bl4
 +[0-9]+: 0+10 +0 TLS +LOCAL  DEFAULT +6 bl5
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN  ABS _DYNAMIC
 +[0-9]+: 0+1c +0 TLS +LOCAL  HIDDEN +6 sh3
 +[0-9]+: 0+20 +0 TLS +LOCAL  HIDDEN +6 sh4
 +[0-9]+: 0+14 +0 TLS +LOCAL  HIDDEN +6 sh1
 +[0-9]+: 0+218c +0 OBJECT  LOCAL  HIDDEN  ABS _GLOBAL_OFFSET_TABLE_
 +[0-9]+: 0+18 +0 TLS +LOCAL  HIDDEN +6 sh2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg4
 +[0-9]+: 0+1000 +0 FUNC +GLOBAL DEFAULT +5 fn3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sg2
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
