#source: tlssunbin32.s
#as: --32
#ld: -shared -melf32_sparc tmpdir/libtlslib32.so tmpdir/tlssunbinpic32.o
#readelf: -WSsrl
#target: sparc*-*-*

.*

Section Headers:
 +\[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
 +\[ 1\] .interp +.*
 +\[ 2\] .hash +.*
 +\[ 3\] .dynsym +.*
 +\[ 4\] .dynstr +.*
 +\[ 5\] .rela.dyn +.*
 +\[ 6\] .text +PROGBITS +0+11000 0+1000 0+1194 00 +AX +0 +0 4096
 +\[ 7\] .tdata +PROGBITS +0+22194 0+2194 0+1060 00 WAT +0 +0 +4
 +\[ 8\] .tbss +NOBITS +0+231f4 0+31f4 0+40 00 WAT +0 +0 +4
 +\[ 9\] .dynamic +DYNAMIC +0+231f4 0+31f4 0+80 08 +WA +4 +0 +4
 +\[10\] .got +PROGBITS +0+23274 0+3274 0+14 04 +WA +0 +0 +4
 +\[11\] .shstrtab +.*
 +\[12\] .symtab +.*
 +\[13\] .strtab +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x12000
There are 6 program headers, starting at offset [0-9]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz MemSiz +Flg Align
 +PHDR +0x0+34 0x0+10034 0x0+10034 0x0+c0 0x0+c0 R E 0x4
 +INTERP +0x0+f4 0x0+100f4 0x0+100f4 0x0+11 0x0+11 R +0x1
.*Requesting program interpreter.*
 +LOAD .* R E 0x10000
 +LOAD .* RW +0x10000
 +DYNAMIC .* RW +0x4
 +TLS .* 0x0+1060 0x0+10a0 R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 4 entries:
 Offset +Info +Type +Sym. Value +Symbol's Name \+ Addend
[0-9a-f ]+R_SPARC_TLS_TPOFF32 +00000000 +sG5 \+ 0
[0-9a-f ]+R_SPARC_TLS_TPOFF32 +00000000 +sG2 \+ 0
[0-9a-f ]+R_SPARC_TLS_TPOFF32 +00000000 +sG6 \+ 0
[0-9a-f ]+R_SPARC_TLS_TPOFF32 +00000000 +sG1 \+ 0

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

Symbol table '.symtab' contains 64 entries:
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
.* OBJECT +LOCAL +HIDDEN +10 _PROCEDURE_LINKAGE_TABLE_
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
