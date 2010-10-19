#source: tlspic1.s
#source: tlspic2.s
#as: --64
#ld: -shared -melf_x86_64
#readelf: -WSsrl
#target: x86_64-*-*

There are 16 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
  \[ 1\] .hash +.*
  \[ 2\] .dynsym +.*
  \[ 3\] .dynstr +.*
  \[ 4\] .rela.dyn +.*
  \[ 5\] .rela.plt +.*
  \[ 6\] .plt +.*
  \[ 7\] .text +PROGBITS +0+1000 0+1000 0+1ac 00 +AX +0 +0 4096
  \[ 8\] .tdata +PROGBITS +0+1011ac 0+11ac 0+60 00 WAT +0 +0 +1
  \[ 9\] .tbss +NOBITS +0+10120c 0+120c 0+20 00 WAT +0 +0 +1
  \[10\] .dynamic +DYNAMIC +0+101210 0+1210 0+130 10 +WA +3 +0 +8
  \[11\] .got +PROGBITS +0+101340 0+1340 0+90 08 +WA +0 +0 +8
  \[12\] .got.plt +PROGBITS +0+1013d0 0+13d0 0+20 08 +WA +0 +0 +8
  \[13\] .shstrtab +.*
  \[14\] .symtab +.*
  \[15\] .strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  LOAD +0x0+ 0x0+ 0x0+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x100000
  LOAD +0x0+11ac 0x0+1011ac 0x0+1011ac 0x0+244 0x0+244 RW +0x100000
  DYNAMIC +0x0+1210 0x0+101210 0x0+101210 0x0+130 0x0+130 RW +0x8
  TLS +0x0+11ac 0x0+1011ac 0x0+1011ac 0x0+60 0x0+80 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 +.hash .dynsym .dynstr .rela.dyn .rela.plt .plt .text *
   01 +.tdata .dynamic .got .got.plt *
   02 +.dynamic *
   03 +.tdata .tbss *

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 14 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_X86_64_DTPMOD64 +0+
[0-9a-f ]+R_X86_64_TPOFF64 +0+24
[0-9a-f ]+R_X86_64_TPOFF64 +0+30
[0-9a-f ]+R_X86_64_DTPMOD64 +0+
[0-9a-f ]+R_X86_64_DTPMOD64 +0+
[0-9a-f ]+R_X86_64_TPOFF64 +0+64
[0-9a-f ]+R_X86_64_TPOFF64 +0+50
[0-9a-f ]+R_X86_64_TPOFF64 +0+70
[0-9a-f ]+R_X86_64_DTPMOD64 +0+
[0-9a-f ]+R_X86_64_TPOFF64 +0+44
[0-9a-f ]+R_X86_64_TPOFF64 +0+10 sg5 \+ 0
[0-9a-f ]+R_X86_64_DTPMOD64 +0+ sg1 \+ 0
[0-9a-f ]+R_X86_64_DTPOFF64 +0+ sg1 \+ 0
[0-9a-f ]+R_X86_64_TPOFF64 +0+4 sg2 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f ]+R_X86_64_JUMP_SLOT +0+ __tls_get_addr \+ 0

Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE  LOCAL  DEFAULT  UND *
.* SECTION LOCAL  DEFAULT +7 *
.* SECTION LOCAL  DEFAULT +8 *
.* SECTION LOCAL  DEFAULT +9 *
.* TLS +GLOBAL DEFAULT +8 sg8
.* TLS +GLOBAL DEFAULT +8 sg3
.* TLS +GLOBAL DEFAULT +8 sg4
.* TLS +GLOBAL DEFAULT +8 sg5
.* NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +8 sg1
.* FUNC +GLOBAL DEFAULT +7 fn1
.* NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.* TLS +GLOBAL DEFAULT +8 sg2
.* TLS +GLOBAL DEFAULT +8 sg6
.* TLS +GLOBAL DEFAULT +8 sg7
.* NOTYPE  GLOBAL DEFAULT  ABS _edata
.* NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 55 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE  LOCAL  DEFAULT  UND *
.* SECTION LOCAL  DEFAULT +1 *
.* SECTION LOCAL  DEFAULT +2 *
.* SECTION LOCAL  DEFAULT +3 *
.* SECTION LOCAL  DEFAULT +4 *
.* SECTION LOCAL  DEFAULT +5 *
.* SECTION LOCAL  DEFAULT +6 *
.* SECTION LOCAL  DEFAULT +7 *
.* SECTION LOCAL  DEFAULT +8 *
.* SECTION LOCAL  DEFAULT +9 *
.* SECTION LOCAL  DEFAULT +10 *
.* SECTION LOCAL  DEFAULT +11 *
.* SECTION LOCAL  DEFAULT +12 *
.* SECTION LOCAL  DEFAULT +13 *
.* SECTION LOCAL  DEFAULT +14 *
.* SECTION LOCAL  DEFAULT +15 *
.* TLS +LOCAL  DEFAULT +8 sl1
.* TLS +LOCAL  DEFAULT +8 sl2
.* TLS +LOCAL  DEFAULT +8 sl3
.* TLS +LOCAL  DEFAULT +8 sl4
.* TLS +LOCAL  DEFAULT +8 sl5
.* TLS +LOCAL  DEFAULT +8 sl6
.* TLS +LOCAL  DEFAULT +8 sl7
.* TLS +LOCAL  DEFAULT +8 sl8
.* TLS +LOCAL  HIDDEN +9 sH1
.* OBJECT +LOCAL +HIDDEN +ABS _DYNAMIC
.* TLS +LOCAL  HIDDEN +8 sh3
.* TLS +LOCAL  HIDDEN +9 sH2
.* TLS +LOCAL  HIDDEN +9 sH7
.* TLS +LOCAL  HIDDEN +8 sh7
.* TLS +LOCAL  HIDDEN +8 sh8
.* TLS +LOCAL  HIDDEN +9 sH4
.* TLS +LOCAL  HIDDEN +8 sh4
.* TLS +LOCAL  HIDDEN +9 sH3
.* TLS +LOCAL  HIDDEN +8 sh5
.* TLS +LOCAL  HIDDEN +9 sH5
.* TLS +LOCAL  HIDDEN +9 sH6
.* TLS +LOCAL  HIDDEN +9 sH8
.* TLS +LOCAL  HIDDEN +8 sh1
.* OBJECT  LOCAL  HIDDEN  ABS _GLOBAL_OFFSET_TABLE_
.* TLS +LOCAL  HIDDEN +8 sh2
.* TLS +LOCAL  HIDDEN +8 sh6
.* TLS +GLOBAL DEFAULT +8 sg8
.* TLS +GLOBAL DEFAULT +8 sg3
.* TLS +GLOBAL DEFAULT +8 sg4
.* TLS +GLOBAL DEFAULT +8 sg5
.* NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +8 sg1
.* FUNC +GLOBAL DEFAULT +7 fn1
.* NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.* TLS +GLOBAL DEFAULT +8 sg2
.* TLS +GLOBAL DEFAULT +8 sg6
.* TLS +GLOBAL DEFAULT +8 sg7
.* NOTYPE  GLOBAL DEFAULT  ABS _edata
.* NOTYPE  GLOBAL DEFAULT  ABS _end
