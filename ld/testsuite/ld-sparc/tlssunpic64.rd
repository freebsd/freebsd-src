#source: tlssunpic64.s
#source: tlspic.s
#as: --64 -Av9 -K PIC
#ld: -shared -melf64_sparc
#readelf: -WSsrl
#target: sparc*-*-*

There are 15 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
 +\[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] .hash +.*
 +\[ 2\] .dynsym +.*
 +\[ 3\] .dynstr +.*
 +\[ 4\] .rela.dyn +.*
 +\[ 5\] .rela.plt +.*
 +\[ 6\] .text +PROGBITS +0+1000 0+1000 0+1000 0+ +AX +0 +0 4096
 +\[ 7\] .tdata +PROGBITS +0+102000 0+2000 0+60 0+ WAT +0 +0 +4
 +\[ 8\] .tbss +NOBITS +0+102060 0+2060 0+20 0+ WAT +0 +0 +4
 +\[ 9\] .dynamic +DYNAMIC +0+102060 0+2060 0+130 10 +WA +3 +0 +8
 +\[10\] .got +PROGBITS +0+102190 0+2190 0+98 08 +WA +0 +0 +8
 +\[11\] .plt +.*
 +\[12\] .shstrtab +.*
 +\[13\] .symtab +.*
 +\[14\] .strtab +.*
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x0+2000 0x0+2000 R E 0x100000
 +LOAD +0x0+2000 0x0+102000 0x0+102000 0x0+3a0 0x0+3a0 RWE 0x100000
 +DYNAMIC +0x0+2060 0x0+102060 0x0+102060 0x0+130 0x0+130 RW +0x8
 +TLS +0x0+2000 0x0+102000 0x0+102000 0x0+60 0x0+80 R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 14 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_SPARC_TLS_DTPMOD64 +0+
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+24
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+30
[0-9a-f ]+R_SPARC_TLS_DTPMOD64 +0+
[0-9a-f ]+R_SPARC_TLS_DTPMOD64 +0+
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+64
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+50
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+70
[0-9a-f ]+R_SPARC_TLS_DTPMOD64 +0+
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+44
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+10 +sg5 \+ 0
[0-9a-f ]+R_SPARC_TLS_DTPMOD64 +0+ +sg1 \+ 0
[0-9a-f ]+R_SPARC_TLS_DTPOFF64 +0+ +sg1 \+ 0
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+4 +sg2 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_SPARC_JMP_SLOT +0+ +__tls_get_addr \+ 0

Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND *
.* SECTION LOCAL +DEFAULT +6 *
.* SECTION LOCAL +DEFAULT +7 *
.* SECTION LOCAL +DEFAULT +8 *
.* SECTION LOCAL +DEFAULT +10 *
.* TLS +GLOBAL DEFAULT +7 sg8
.* TLS +GLOBAL DEFAULT +7 sg3
.* TLS +GLOBAL DEFAULT +7 sg4
.* TLS +GLOBAL DEFAULT +7 sg5
.* NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +7 sg1
.* FUNC +GLOBAL DEFAULT +6 fn1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* TLS +GLOBAL DEFAULT +7 sg2
.* TLS +GLOBAL DEFAULT +7 sg6
.* TLS +GLOBAL DEFAULT +7 sg7
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 55 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND *
.* SECTION LOCAL +DEFAULT +1 *
.* SECTION LOCAL +DEFAULT +2 *
.* SECTION LOCAL +DEFAULT +3 *
.* SECTION LOCAL +DEFAULT +4 *
.* SECTION LOCAL +DEFAULT +5 *
.* SECTION LOCAL +DEFAULT +6 *
.* SECTION LOCAL +DEFAULT +7 *
.* SECTION LOCAL +DEFAULT +8 *
.* SECTION LOCAL +DEFAULT +9 *
.* SECTION LOCAL +DEFAULT +10 *
.* SECTION LOCAL +DEFAULT +11 *
.* SECTION LOCAL +DEFAULT +12 *
.* SECTION LOCAL +DEFAULT +13 *
.* SECTION LOCAL +DEFAULT +14 *
.* TLS +LOCAL +DEFAULT +7 sl1
.* TLS +LOCAL +DEFAULT +7 sl2
.* TLS +LOCAL +DEFAULT +7 sl3
.* TLS +LOCAL +DEFAULT +7 sl4
.* TLS +LOCAL +DEFAULT +7 sl5
.* TLS +LOCAL +DEFAULT +7 sl6
.* TLS +LOCAL +DEFAULT +7 sl7
.* TLS +LOCAL +DEFAULT +7 sl8
.* TLS +LOCAL +HIDDEN +8 sH1
.* OBJECT +LOCAL +HIDDEN +ABS _DYNAMIC
.* TLS +LOCAL +HIDDEN +7 sh3
.* TLS +LOCAL +HIDDEN +8 sH2
.* TLS +LOCAL +HIDDEN +8 sH7
.* OBJECT +LOCAL +HIDDEN +ABS _PROCEDURE_LINKAGE_TABLE_
.* TLS +LOCAL +HIDDEN +7 sh7
.* TLS +LOCAL +HIDDEN +7 sh8
.* TLS +LOCAL +HIDDEN +8 sH4
.* TLS +LOCAL +HIDDEN +7 sh4
.* TLS +LOCAL +HIDDEN +8 sH3
.* TLS +LOCAL +HIDDEN +7 sh5
.* TLS +LOCAL +HIDDEN +8 sH5
.* TLS +LOCAL +HIDDEN +8 sH6
.* TLS +LOCAL +HIDDEN +8 sH8
.* TLS +LOCAL +HIDDEN +7 sh1
.* OBJECT +LOCAL  HIDDEN +ABS _GLOBAL_OFFSET_TABLE_
.* TLS +LOCAL +HIDDEN +7 sh2
.* TLS +LOCAL +HIDDEN +7 sh6
.* TLS +GLOBAL DEFAULT +7 sg8
.* TLS +GLOBAL DEFAULT +7 sg3
.* TLS +GLOBAL DEFAULT +7 sg4
.* TLS +GLOBAL DEFAULT +7 sg5
.* NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +7 sg1
.* FUNC +GLOBAL DEFAULT +6 fn1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* TLS +GLOBAL DEFAULT +7 sg2
.* TLS +GLOBAL DEFAULT +7 sg6
.* TLS +GLOBAL DEFAULT +7 sg7
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
