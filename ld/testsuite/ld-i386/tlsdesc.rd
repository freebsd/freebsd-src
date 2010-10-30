#source: tlsdesc.s
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
  \[ 6\] \.text +.*
  \[ 7\] \.tdata +PROGBITS +[0-9a-f]+ [0-9a-f]+ 000060 00 WAT  0   0  1
  \[ 8\] \.tbss +NOBITS +[0-9aa-f]+ [0-9a-f]+ 000020 00 WAT  0   0  1
  \[ 9\] \.dynamic +.*
  \[10\] \.got +.*
  \[11\] \.got.plt +.*
  \[12\] \.shstrtab +.*
  \[13\] \.symtab +.*
  \[14\] \.strtab +.*
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
   00 +.hash .dynsym .dynstr .rel.dyn .rel.plt .text *
   01 +.tdata .dynamic .got .got.plt *
   02 +.dynamic *
   03 +.tdata .tbss *

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 20 entries:
 Offset +Info +Type +Sym.Value +Sym. Name
[0-9a-f ]+R_386_TLS_TPOFF32
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF32
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF32
[0-9a-f ]+R_386_TLS_TPOFF32
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF32
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF *
[0-9a-f ]+R_386_TLS_TPOFF32
[0-9a-f ]+R_386_TLS_TPOFF   0+8   sg3
[0-9a-f ]+R_386_TLS_TPOFF32 0+c   sg4
[0-9a-f ]+R_386_TLS_TPOFF   0+c   sg4
[0-9a-f ]+R_386_TLS_TPOFF   0+10   sg5
[0-9a-f ]+R_386_TLS_TPOFF32 0+4   sg2

Relocation section '.rel.plt' at offset 0x[0-9a-f]+ contains 5 entries:
 Offset     Info    Type            Sym.Value  Sym. Name
[0-9a-f ]+R_386_TLS_DESC * 0+   sg1
[0-9a-f ]+R_386_TLS_DESC *
[0-9a-f ]+R_386_TLS_DESC *
[0-9a-f ]+R_386_TLS_DESC *
[0-9a-f ]+R_386_TLS_DESC *

Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: + Value  Size Type + Bind +Vis +Ndx Name
 +[0-9]+: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +[0-9]+: 0+1c +0 TLS +GLOBAL DEFAULT +7 sg8
 +[0-9]+: 0+8 +0 TLS +GLOBAL DEFAULT +7 sg3
 +[0-9]+: 0+c +0 TLS +GLOBAL DEFAULT +7 sg4
 +[0-9]+: 0+10 +0 TLS +GLOBAL DEFAULT +7 sg5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT +7 sg1
 +[0-9]+: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +6 fn1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+4 +0 TLS +GLOBAL DEFAULT +7 sg2
 +[0-9]+: 0+14 +0 TLS +GLOBAL DEFAULT +7 sg6
 +[0-9]+: 0+18 +0 TLS +GLOBAL DEFAULT +7 sg7
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 51 entries:
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
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +10 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +11 *
 +[0-9]+: 0+20 +0 TLS +LOCAL  DEFAULT +7 sl1
 +[0-9]+: 0+24 +0 TLS +LOCAL  DEFAULT +7 sl2
 +[0-9]+: 0+28 +0 TLS +LOCAL  DEFAULT +7 sl3
 +[0-9]+: 0+2c +0 TLS +LOCAL  DEFAULT +7 sl4
 +[0-9]+: 0+30 +0 TLS +LOCAL  DEFAULT +7 sl5
 +[0-9]+: 0+34 +0 TLS +LOCAL  DEFAULT +7 sl6
 +[0-9]+: 0+38 +0 TLS +LOCAL  DEFAULT +7 sl7
 +[0-9]+: 0+3c +0 TLS +LOCAL  DEFAULT +7 sl8
 +[0-9]+: 0+60 +0 TLS +LOCAL  HIDDEN +8 sH1
 +[0-9]+: 0+ +0 TLS +LOCAL  HIDDEN +7 _TLS_MODULE_BASE_
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN  ABS _DYNAMIC
 +[0-9]+: 0+48 +0 TLS +LOCAL  HIDDEN +7 sh3
 +[0-9]+: 0+64 +0 TLS +LOCAL  HIDDEN +8 sH2
 +[0-9]+: 0+78 +0 TLS +LOCAL  HIDDEN +8 sH7
 +[0-9]+: 0+58 +0 TLS +LOCAL  HIDDEN +7 sh7
 +[0-9]+: 0+5c +0 TLS +LOCAL  HIDDEN +7 sh8
 +[0-9]+: 0+6c +0 TLS +LOCAL  HIDDEN +8 sH4
 +[0-9]+: 0+4c +0 TLS +LOCAL  HIDDEN +7 sh4
 +[0-9]+: 0+68 +0 TLS +LOCAL  HIDDEN +8 sH3
 +[0-9]+: 0+50 +0 TLS +LOCAL  HIDDEN +7 sh5
 +[0-9]+: 0+70 +0 TLS +LOCAL  HIDDEN +8 sH5
 +[0-9]+: 0+74 +0 TLS +LOCAL  HIDDEN +8 sH6
 +[0-9]+: 0+7c +0 TLS +LOCAL  HIDDEN +8 sH8
 +[0-9]+: 0+40 +0 TLS +LOCAL  HIDDEN +7 sh1
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN  ABS _GLOBAL_OFFSET_TABLE_
 +[0-9]+: 0+44 +0 TLS +LOCAL  HIDDEN +7 sh2
 +[0-9]+: 0+54 +0 TLS +LOCAL  HIDDEN +7 sh6
 +[0-9]+: 0+1c +0 TLS +GLOBAL DEFAULT +7 sg8
 +[0-9]+: 0+8 +0 TLS +GLOBAL DEFAULT +7 sg3
 +[0-9]+: 0+c +0 TLS +GLOBAL DEFAULT +7 sg4
 +[0-9]+: 0+10 +0 TLS +GLOBAL DEFAULT +7 sg5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT +7 sg1
 +[0-9]+: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +6 fn1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+4 +0 TLS +GLOBAL DEFAULT +7 sg2
 +[0-9]+: 0+14 +0 TLS +GLOBAL DEFAULT +7 sg6
 +[0-9]+: 0+18 +0 TLS +GLOBAL DEFAULT +7 sg7
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
