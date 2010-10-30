#source: tlsgdesc.s
#as: --64
#ld: -shared -melf64_x86_64
#readelf: -WSsrl
#target: x86_64-*-*

There are [0-9]+ section headers, starting at offset 0x.*:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] \.hash +.*
  \[ 2\] \.dynsym +.*
  \[ 3\] \.dynstr +.*
  \[ 4\] \.rela.dyn +.*
  \[ 5\] \.rela.plt +.*
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
   00 +.hash .dynsym .dynstr .rela.dyn .rela.plt .plt .text *
   01 +.dynamic .got .got.plt *
   02 +.dynamic *

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 8 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f]+  0+200000012 R_X86_64_TPOFF64 +0+ sG3 \+ 0
[0-9a-f]+  0+300000012 R_X86_64_TPOFF64 +0+ sG5 \+ 0
[0-9a-f]+  0+400000010 R_X86_64_DTPMOD64 +0+ sG2 \+ 0
[0-9a-f]+  0+400000011 R_X86_64_DTPOFF64 +0+ sG2 \+ 0
[0-9a-f]+  0+500000012 R_X86_64_TPOFF64 +0+ sG4 \+ 0
[0-9a-f]+  0+800000012 R_X86_64_TPOFF64 +0+ sG6 \+ 0
[0-9a-f]+  0+a00000010 R_X86_64_DTPMOD64 +0+ sG1 \+ 0
[0-9a-f]+  0+a00000011 R_X86_64_DTPOFF64 +0+ sG1 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 3 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f]+  0+600000007 R_X86_64_JUMP_SLOT +0+ __tls_get_addr \+ 0
[0-9a-f]+  0+a00000024 R_X86_64_TLSDESC +0+ sG1 \+ 0
[0-9a-f]+  0+400000024 R_X86_64_TLSDESC +0+ sG2 \+ 0

Symbol table '.dynsym' contains 13 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +[0-9]+: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 *
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +[0-9]+: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +[0-9]+: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +7 fc1
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 24 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
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
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN  ABS _DYNAMIC
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN  ABS _GLOBAL_OFFSET_TABLE_
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +[0-9]+: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +[0-9]+: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +7 fc1
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
