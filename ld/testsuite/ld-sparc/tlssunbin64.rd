#source: tlssunbin64.s
#as: --64
#ld: -shared -melf64_sparc tmpdir/libtlslib64.so tmpdir/tlssunbinpic64.o
#readelf: -WSsrl
#target: sparc*-*-*

.*

Section Headers:
 +\[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
 +\[ 1\] .interp +.*
 +\[ 2\] .hash +.*
 +\[ 3\] .dynsym +.*
 +\[ 4\] .dynstr +.*
 +\[ 5\] .rela.dyn +.*
 +\[ 6\] .text +PROGBITS +0+101000 0+1000 0+11a4 00 +AX +0 +0 4096
 +\[ 7\] .tdata +PROGBITS +0+2021a4 0+21a4 0+0060 00 WAT +0 +0 +4
 +\[ 8\] .tbss +NOBITS +0+202204 0+2204 0+40 00 WAT +0 +0 +4
 +\[ 9\] .dynamic +DYNAMIC +0+202208 0+2208 0+100 10 +WA +4 +0 +8
 +\[10\] .got +PROGBITS +0+202308 0+2308 0+28 08 +WA +0 +0 +8
 +\[11\] .shstrtab +.*
 +\[12\] .symtab +.*
 +\[13\] .strtab +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x102000
There are 6 program headers, starting at offset [0-9]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +PHDR +0x0+40 0x0+100040 0x0+100040 0x0+150 0x0+150 R E 0x8
 +INTERP +0x0+190 0x0+100190 0x0+100190 0x0+19 0x0+19 R +0x1
.*Requesting program interpreter.*
 +LOAD .* R E 0x100000
 +LOAD .* RW +0x100000
 +DYNAMIC .* RW +0x8
 +TLS .* 0x0+60 0x0+a0 R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 4 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+ +sG5 \+ 0
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+ +sG2 \+ 0
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+ +sG6 \+ 0
[0-9a-f ]+R_SPARC_TLS_TPOFF64 +0+ +sG1 \+ 0

Symbol table '.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND *
.* TLS +GLOBAL DEFAULT +UND sG5
.* TLS +GLOBAL DEFAULT +UND sG2
.* FUNC +GLOBAL DEFAULT +UND __tls_get_addr
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* TLS +GLOBAL DEFAULT +UND sG6
.* TLS +GLOBAL DEFAULT +UND sG1
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 67 entries:
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
.* TLS +LOCAL +DEFAULT +7 sl1
.* TLS +LOCAL +DEFAULT +7 sl2
.* TLS +LOCAL +DEFAULT +7 sl3
.* TLS +LOCAL +DEFAULT +7 sl4
.* TLS +LOCAL +DEFAULT +7 sl5
.* TLS +LOCAL +DEFAULT +7 sl6
.* TLS +LOCAL +DEFAULT +7 sl7
.* TLS +LOCAL +DEFAULT +7 sl8
.* TLS +LOCAL +DEFAULT +8 bl1
.* TLS +LOCAL +DEFAULT +8 bl2
.* TLS +LOCAL +DEFAULT +8 bl3
.* TLS +LOCAL +DEFAULT +8 bl4
.* TLS +LOCAL +DEFAULT +8 bl5
.* TLS +LOCAL +DEFAULT +8 bl6
.* TLS +LOCAL +DEFAULT +8 bl7
.* TLS +LOCAL +DEFAULT +8 bl8
.* OBJECT +LOCAL +HIDDEN +9 _DYNAMIC
.* OBJECT +LOCAL +HIDDEN +ABS _PROCEDURE_LINKAGE_TABLE_
.* OBJECT +LOCAL +HIDDEN +10 _GLOBAL_OFFSET_TABLE_
.* TLS +GLOBAL DEFAULT +7 sg8
.* TLS +GLOBAL DEFAULT +8 bg8
.* TLS +GLOBAL DEFAULT +8 bg6
.* TLS +GLOBAL DEFAULT +UND sG5
.* TLS +GLOBAL DEFAULT +8 bg3
.* TLS +GLOBAL DEFAULT +7 sg3
.* TLS +GLOBAL HIDDEN +7 sh3
.* TLS +GLOBAL DEFAULT +UND sG2
.* TLS +GLOBAL DEFAULT +7 sg4
.* TLS +GLOBAL DEFAULT +7 sg5
.* TLS +GLOBAL DEFAULT +8 bg5
.* FUNC +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL HIDDEN +7 sh7
.* TLS +GLOBAL HIDDEN +7 sh8
.* TLS +GLOBAL DEFAULT +7 sg1
.* FUNC +GLOBAL DEFAULT +6 _start
.* TLS +GLOBAL HIDDEN +7 sh4
.* TLS +GLOBAL DEFAULT +8 bg7
.* TLS +GLOBAL HIDDEN +7 sh5
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* TLS +GLOBAL DEFAULT +UND sG6
.* FUNC +GLOBAL DEFAULT +6 fn2
.* TLS +GLOBAL DEFAULT +7 sg2
.* TLS +GLOBAL DEFAULT +UND sG1
.* TLS +GLOBAL HIDDEN +7 sh1
.* TLS +GLOBAL DEFAULT +7 sg6
.* TLS +GLOBAL DEFAULT +7 sg7
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
.* TLS +GLOBAL HIDDEN +7 sh2
.* TLS +GLOBAL HIDDEN +7 sh6
.* TLS +GLOBAL DEFAULT +8 bg2
.* TLS +GLOBAL DEFAULT +8 bg1
.* TLS +GLOBAL DEFAULT +8 bg4
