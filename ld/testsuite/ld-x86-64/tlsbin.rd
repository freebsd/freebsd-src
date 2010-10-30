#source: tlsbinpic.s
#source: tlsbin.s
#as: --64
#ld: -shared -melf_x86_64
#readelf: -WSsrl
#target: x86_64-*-*

There are 17 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
  \[ 1\] .interp +.*
  \[ 2\] .hash +.*
  \[ 3\] .dynsym +.*
  \[ 4\] .dynstr +.*
  \[ 5\] .rela.dyn +.*
  \[ 6\] .rela.plt +.*
  \[ 7\] .plt +.*
  \[ 8\] .text +PROGBITS +0+401000 0+1000 0+22a 00 +AX +0 +0 +4096
  \[ 9\] .tdata +PROGBITS +0+60122a 0+122a 0+60 00 WAT +0 +0 +1
  \[10\] .tbss +NOBITS +0+60128a 0+128a 0+40 00 WAT +0 +0 +1
  \[11\] .dynamic +DYNAMIC +0+601290 0+1290 0+140 10 +WA +4 +0 +8
  \[12\] .got +PROGBITS +0+6013d0 0+13d0 0+20 08 +WA +0 +0 +8
  \[13\] .got.plt +PROGBITS +0+6013f0 0+13f0 0+20 08 +WA +0 +0 +8
  \[14\] .shstrtab +.*
  \[15\] .symtab +.*
  \[16\] .strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is EXEC \(Executable file\)
Entry point 0x40113c
There are 6 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  PHDR.*
  INTERP.*
.*Requesting program interpreter.*
  LOAD +0x0+ 0x0+400000 0x0+400000 0x0+122a 0x0+122a R E 0x200000
  LOAD +0x0+122a 0x0+60122a 0x0+60122a 0x0+1e6 0x0+1e6 RW  0x200000
  DYNAMIC +0x0+1290 0x0+601290 0x0+601290 0x0+140 0x0+140 RW  0x8
  TLS +0x0+122a 0x0+60122a 0x0+60122a 0x0+60 0x0+a0 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 *
   01 +.interp *
   02 +.interp .hash .dynsym .dynstr .rela.dyn .rela.plt .plt .text *
   03 +.tdata .dynamic .got .got.plt *
   04 +.dynamic *
   05 +.tdata .tbss *

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 4 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f ]+R_X86_64_TPOFF64 +0+ sG5 \+ 0
[0-9a-f ]+R_X86_64_TPOFF64 +0+ sG2 \+ 0
[0-9a-f ]+R_X86_64_TPOFF64 +0+ sG6 \+ 0
[0-9a-f ]+R_X86_64_TPOFF64 +0+ sG1 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f ]+R_X86_64_JUMP_SLOT[0-9a-f ]+__tls_get_addr \+ 0

Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE  LOCAL  DEFAULT  UND *
.* TLS +GLOBAL DEFAULT  UND sG5
.* TLS +GLOBAL DEFAULT  UND sG2
.* FUNC +GLOBAL DEFAULT  UND __tls_get_addr
.* NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.* TLS +GLOBAL DEFAULT  UND sG6
.* TLS +GLOBAL DEFAULT  UND sG1
.* NOTYPE  GLOBAL DEFAULT  ABS _edata
.* NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 66 entries:
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
.* TLS +LOCAL  DEFAULT +9 sl1
.* TLS +LOCAL  DEFAULT +9 sl2
.* TLS +LOCAL  DEFAULT +9 sl3
.* TLS +LOCAL  DEFAULT +9 sl4
.* TLS +LOCAL  DEFAULT +9 sl5
.* TLS +LOCAL  DEFAULT +9 sl6
.* TLS +LOCAL  DEFAULT +9 sl7
.* TLS +LOCAL  DEFAULT +9 sl8
.* TLS +LOCAL  DEFAULT +10 bl1
.* TLS +LOCAL  DEFAULT +10 bl2
.* TLS +LOCAL  DEFAULT +10 bl3
.* TLS +LOCAL  DEFAULT +10 bl4
.* TLS +LOCAL  DEFAULT +10 bl5
.* TLS +LOCAL  DEFAULT +10 bl6
.* TLS +LOCAL  DEFAULT +10 bl7
.* TLS +LOCAL  DEFAULT +10 bl8
.* OBJECT  LOCAL  HIDDEN +11 _DYNAMIC
.* OBJECT  LOCAL  HIDDEN +13 _GLOBAL_OFFSET_TABLE_
.* TLS +GLOBAL DEFAULT +9 sg8
.* TLS +GLOBAL DEFAULT +10 bg8
.* TLS +GLOBAL DEFAULT +10 bg6
.* TLS +GLOBAL DEFAULT  UND sG5
.* TLS +GLOBAL DEFAULT +10 bg3
.* TLS +GLOBAL DEFAULT +9 sg3
.* TLS +GLOBAL HIDDEN +9 sh3
.* TLS +GLOBAL DEFAULT  UND sG2
.* TLS +GLOBAL DEFAULT +9 sg4
.* TLS +GLOBAL DEFAULT +9 sg5
.* TLS +GLOBAL DEFAULT +10 bg5
.* FUNC +GLOBAL DEFAULT  UND __tls_get_addr
.* TLS +GLOBAL HIDDEN +9 sh7
.* TLS +GLOBAL HIDDEN +9 sh8
.* TLS +GLOBAL DEFAULT +9 sg1
.* FUNC +GLOBAL DEFAULT +8 _start
.* TLS +GLOBAL HIDDEN +9 sh4
.* TLS +GLOBAL DEFAULT +10 bg7
.* TLS +GLOBAL HIDDEN +9 sh5
.* NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.* TLS +GLOBAL DEFAULT  UND sG6
.* FUNC +GLOBAL DEFAULT +8 fn2
.* TLS +GLOBAL DEFAULT +9 sg2
.* TLS +GLOBAL DEFAULT  UND sG1
.* TLS +GLOBAL HIDDEN +9 sh1
.* TLS +GLOBAL DEFAULT +9 sg6
.* TLS +GLOBAL DEFAULT +9 sg7
.* NOTYPE  GLOBAL DEFAULT  ABS _edata
.* NOTYPE  GLOBAL DEFAULT  ABS _end
.* TLS +GLOBAL HIDDEN +9 sh2
.* TLS +GLOBAL HIDDEN +9 sh6
.* TLS +GLOBAL DEFAULT +10 bg2
.* TLS +GLOBAL DEFAULT +10 bg1
.* TLS +GLOBAL DEFAULT +10 bg4
