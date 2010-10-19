#source: tlsgdesc.s
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
  \[ 8\] \.dynamic +.*
  \[ 9\] \.got +.*
  \[10\] \.got.plt +.*
  \[11\] \.shstrtab +.*
  \[12\] \.symtab +.*
  \[13\] \.strtab +.*
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

 Section to Segment mapping:
  Segment Sections...
   00 +.hash .dynsym .dynstr .rel.dyn .rel.plt .plt .text *
   01 +.dynamic .got .got.plt *
   02 +.dynamic *

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 8 entries:
 Offset +Info +Type +Sym.Value +Sym. Name
[0-9a-f]+ +0+225 R_386_TLS_TPOFF32 0+   sG3
[0-9a-f]+ +0+30e R_386_TLS_TPOFF   0+   sG5
[0-9a-f]+ +0+423 R_386_TLS_DTPMOD3 0+   sG2
[0-9a-f]+ +0+424 R_386_TLS_DTPOFF3 0+   sG2
[0-9a-f]+ +0+50e R_386_TLS_TPOFF   0+   sG4
[0-9a-f]+ +0+725 R_386_TLS_TPOFF32 0+   sG6
[0-9a-f]+ +0+923 R_386_TLS_DTPMOD3 0+   sG1
[0-9a-f]+ +0+924 R_386_TLS_DTPOFF3 0+   sG1

Relocation section '.rel.plt' at offset 0x[0-9a-f]+ contains 3 entries:
 Offset     Info    Type            Sym.Value  Sym. Name
[0-9a-f]+  0+c07 R_386_JUMP_SLOT   0+   ___tls_get_addr
[0-9a-f]+  0+929 R_386_TLS_DESC    0+   sG1
[0-9a-f]+  0+429 R_386_TLS_DESC    0+   sG2

Symbol table '.dynsym' contains 13 entries:
 +Num: + Value  Size Type + Bind +Vis +Ndx Name
 +[0-9]+: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 *
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +[0-9]+: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +7 fc1
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +[0-9]+: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND ___tls_get_addr

Symbol table '.symtab' contains 27 entries:
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
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +12 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +13 *
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN  ABS _DYNAMIC
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN  ABS _GLOBAL_OFFSET_TABLE_
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +[0-9]+: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +7 fc1
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +[0-9]+: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND ___tls_get_addr
