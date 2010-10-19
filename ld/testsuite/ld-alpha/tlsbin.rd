#source: align.s
#source: tlsbinpic.s
#source: tlsbin.s
#as:
#ld: -melf64alpha
#readelf: -WSsrl
#target: alpha*-*-*

There are [0-9]* section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
  \[ 1] .interp +.*
  \[ 2\] .hash +.*
  \[ 3\] .dynsym +.*
  \[ 4\] .dynstr +.*
  \[ 5\] .rela.dyn +.*
  \[ 6\] .rela.plt +.*
  \[ 7\] .text +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 +AX +0 +0 4096
  \[ 8\] .eh_frame +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 +A +0 +0 +8
  \[ 9\] .tdata +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 WAT +0 +0 +4
  \[10\] .tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 WAT +0 +0 +1
  \[11\] .dynamic +DYNAMIC +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 10 +WA +4 +0 +8
  \[12\] .plt +.*
  \[13\] .got +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 +WA +0 +0 +8
  \[14\] .shstrtab +.*
  \[15\] .symtab +.*
  \[16\] .strtab +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x[0-9a-f]+
There are 6 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  PHDR +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x0+150 R E 0x8
  INTERP +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ R +0x1
.*Requesting program interpreter.*
  LOAD +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x10000
  LOAD +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RWE 0x10000
  DYNAMIC +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RW +0x8
  TLS +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 3 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f]+  [0-9a-f]+ R_ALPHA_TPREL64 +0+ sG2 \+ 0
[0-9a-f]+  [0-9a-f]+ R_ALPHA_DTPMOD64 +0+ sG1 \+ 0
[0-9a-f]+  [0-9a-f]+ R_ALPHA_DTPREL64 +0+ sG1 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
[0-9a-f]+  [0-9a-f]+ R_ALPHA_JMP_SLOT +[0-9a-f]+ __tls_get_addr \+ 0

Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
[0-9 ]+: [0-9a-f]+ +0 NOTYPE +LOCAL +DEFAULT +UND *
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +UND sG2
[0-9 ]+: [0-9a-f]+ +4 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
[0-9 ]+: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +UND sG1
[0-9 ]+: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
[0-9 ]+: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
[0-9 ]+: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +1 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +2 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +3 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +4 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +5 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +6 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +7 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +8 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +9 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +10 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +11 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +12 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +13 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +14 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +15 
[0-9 ]+: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +16 
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl1
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl2
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl3
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl4
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl5
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl6
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl7
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl8
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl1
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl2
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl3
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl4
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl5
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl6
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl7
[0-9 ]+: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl8
[0-9 ]+: [0-9a-f]+ +0 OBJECT +LOCAL +HIDDEN +11 _DYNAMIC
[0-9 ]+: [0-9a-f]+ +0 OBJECT +LOCAL +HIDDEN +12 _PROCEDURE_LINKAGE_TABLE_
[0-9 ]+: [0-9a-f]+ +0 OBJECT +LOCAL +HIDDEN +13 _GLOBAL_OFFSET_TABLE_
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg8
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg8
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg6
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg3
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg3
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh3
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +UND sG2
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg4
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg5
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg5
[0-9 ]+: [0-9a-f]+ +4 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh7
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh8
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg1
[0-9 ]+: [0-9a-f]+ +52 FUNC +GLOBAL DEFAULT +7 _start
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh4
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg7
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh5
[0-9 ]+: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
[0-9 ]+: [0-9a-f]+ +136 FUNC +GLOBAL DEFAULT +7 fn2
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg2
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +UND sG1
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh1
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg6
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg7
[0-9 ]+: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
[0-9 ]+: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh2
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh6
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg2
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg1
[0-9 ]+: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg4
