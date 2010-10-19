#source: tlspic1.s
#source: tlspic2.s
#as: -m64 -Aesame
#ld: -shared -melf64_s390
#readelf: -WSsrl
#target: s390x-*-*

There are 15 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] .hash .*
  \[ 2\] .dynsym .*
  \[ 3\] .dynstr .*
  \[ 4\] .rela.dyn .*
  \[ 5\] .rela.plt .*
  \[ 6\] .plt .*
  \[ 7\] .text +PROGBITS .*
  \[ 8\] .tdata +PROGBITS .* 0+60 00 WAT +0 +0 +32
  \[ 9\] .tbss +NOBITS .* 0+20 00 WAT +0 +0 +1
  \[10\] .dynamic +DYNAMIC .*
  \[11\] .got +PROGBITS .*
  \[12\] .shstrtab .*
  \[13\] .symtab .*
  \[14\] .strtab .*
Key to Flags:
.*
.*
.*

Elf file type is DYN \(Shared object file\)
Entry point 0x[0-9a-f]+
There are 4 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  LOAD .* R E 0x1000
  LOAD .* RW +0x1000
  DYNAMIC .* RW +0x8
  TLS .* 0x0+60 0x0+80 R +0x20

 Section to Segment mapping:
  Segment Sections...
   00 +.hash .dynsym .dynstr .rela.dyn .rela.plt .plt .text *
   01 +.tdata .dynamic .got *
   02 +.dynamic *
   03 +.tdata .tbss *

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 14 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f ]+R_390_TLS_DTPMOD +0+
[0-9a-f ]+R_390_TLS_TPOFF +0+24
[0-9a-f ]+R_390_TLS_TPOFF +0+30
[0-9a-f ]+R_390_TLS_DTPMOD +0+
[0-9a-f ]+R_390_TLS_DTPMOD +0+
[0-9a-f ]+R_390_TLS_TPOFF +0+64
[0-9a-f ]+R_390_TLS_TPOFF +0+50
[0-9a-f ]+R_390_TLS_TPOFF +0+70
[0-9a-f ]+R_390_TLS_DTPMOD +0+
[0-9a-f ]+R_390_TLS_TPOFF +0+44
[0-9a-f ]+R_390_TLS_TPOFF +0+10 sg5 \+ 0
[0-9a-f ]+R_390_TLS_DTPMOD +0+ sg1 \+ 0
[0-9a-f ]+R_390_TLS_DTPOFF +0+ sg1 \+ 0
[0-9a-f ]+R_390_TLS_TPOFF +0+4 sg2 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f ]+R_390_JMP_SLOT +0+ __tls_get_offset \+ 0

Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE  LOCAL  DEFAULT  UND 
.* SECTION LOCAL  DEFAULT +7 
.* SECTION LOCAL  DEFAULT +8 
.* SECTION LOCAL  DEFAULT +9 
.* TLS +GLOBAL DEFAULT +8 sg8
.* TLS +GLOBAL DEFAULT +8 sg3
.* TLS +GLOBAL DEFAULT +8 sg4
.* TLS +GLOBAL DEFAULT +8 sg5
.* NOTYPE  GLOBAL DEFAULT  UND __tls_get_offset
.* TLS +GLOBAL DEFAULT +8 sg1
.* FUNC +GLOBAL DEFAULT +7 fn1
.* NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.* TLS +GLOBAL DEFAULT +8 sg2
.* TLS +GLOBAL DEFAULT +8 sg6
.* TLS +GLOBAL DEFAULT +8 sg7
.* NOTYPE  GLOBAL DEFAULT  ABS _edata
.* NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 54 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE  LOCAL  DEFAULT  UND 
.* SECTION LOCAL  DEFAULT +1 
.* SECTION LOCAL  DEFAULT +2 
.* SECTION LOCAL  DEFAULT +3 
.* SECTION LOCAL  DEFAULT +4 
.* SECTION LOCAL  DEFAULT +5 
.* SECTION LOCAL  DEFAULT +6 
.* SECTION LOCAL  DEFAULT +7 
.* SECTION LOCAL  DEFAULT +8 
.* SECTION LOCAL  DEFAULT +9 
.* SECTION LOCAL  DEFAULT +10 
.* SECTION LOCAL  DEFAULT +11 
.* SECTION LOCAL  DEFAULT +12 
.* SECTION LOCAL  DEFAULT +13 
.* SECTION LOCAL  DEFAULT +14 
.* TLS +LOCAL  DEFAULT +8 sl1
.* TLS +LOCAL  DEFAULT +8 sl2
.* TLS +LOCAL  DEFAULT +8 sl3
.* TLS +LOCAL  DEFAULT +8 sl4
.* TLS +LOCAL  DEFAULT +8 sl5
.* TLS +LOCAL  DEFAULT +8 sl6
.* TLS +LOCAL  DEFAULT +8 sl7
.* TLS +LOCAL  DEFAULT +8 sl8
.* TLS +LOCAL  HIDDEN +9 sH1
.* OBJECT  LOCAL  HIDDEN  ABS _DYNAMIC
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
.* NOTYPE  GLOBAL DEFAULT  UND __tls_get_offset
.* TLS +GLOBAL DEFAULT +8 sg1
.* FUNC +GLOBAL DEFAULT +7 fn1
.* NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.* TLS +GLOBAL DEFAULT +8 sg2
.* TLS +GLOBAL DEFAULT +8 sg6
.* TLS +GLOBAL DEFAULT +8 sg7
.* NOTYPE  GLOBAL DEFAULT  ABS _edata
.* NOTYPE  GLOBAL DEFAULT  ABS _end
