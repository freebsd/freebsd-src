#source: tlssunpic32.s
#source: tlspic.s
#as: --32 -K PIC
#ld: -shared -melf32_sparc
#readelf: -WSsrl
#target: sparc*-*-*

There are 17 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
 +\[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] .hash +.*
 +\[ 2\] .dynsym +.*
 +\[ 3\] .dynstr +.*
 +\[ 4\] .rela.dyn +.*
 +\[ 5\] .rela.plt +.*
 +\[ 6\] .text +PROGBITS +0+1000 0+1000 0+1000 0+ +AX +0 +0 4096
 +\[ 7\] .data +PROGBITS +0+12000 0+2000 0+ 0+ +WA +0 +0 4096
 +\[ 8\] .tdata +PROGBITS +0+12000 0+2000 0+60 0+ WAT +0 +0 +4
 +\[ 9\] .tbss +NOBITS +0+12060 0+2060 0+20 0+ WAT +0 +0 +4
 +\[10\] .dynamic +DYNAMIC +0+12060 0+2060 0+98 08 +WA +3 +0 +4
 +\[11\] .plt +.*
 +\[12\] .got +PROGBITS +0+12138 0+2138 0+4c 04 +WA +0 +0 +4
 +\[13\] .bss +.*
 +\[14\] .shstrtab +.*
 +\[15\] .symtab +.*
 +\[16\] .strtab +.*
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x0+2000 0x0+2000 R E 0x10000
 +LOAD +0x0+2000 0x0+12000 0x0+12000 0x0+184 0x0+188 RWE 0x10000
 +DYNAMIC +0x0+2060 0x0+12060 0x0+12060 0x0+98 0x0+98 RW +0x4
 +TLS +0x0+2000 0x0+12000 0x0+12000 0x0+60 0x0+80 R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 14 entries:
 Offset +Info +Type +Sym. Value +Symbol's Name \+ Addend
0+1213c +0+4a R_SPARC_TLS_DTPMOD32 +0+
0+12144 +0+4e R_SPARC_TLS_TPOFF32 +0+24
0+12148 +0+4e R_SPARC_TLS_TPOFF32 +0+30
0+1214c +0+4a R_SPARC_TLS_DTPMOD32 +0+
0+12154 +0+4a R_SPARC_TLS_DTPMOD32 +0+
0+1215c +0+4e R_SPARC_TLS_TPOFF32 +0+64
0+1216c +0+4e R_SPARC_TLS_TPOFF32 +0+50
0+12170 +0+4e R_SPARC_TLS_TPOFF32 +0+70
0+12178 +0+4a R_SPARC_TLS_DTPMOD32 +0+
0+12180 +0+4e R_SPARC_TLS_TPOFF32 +0+44
0+12160 +0+124e R_SPARC_TLS_TPOFF32 +0+10 +sg5 \+ 0
0+12164 +0+154a R_SPARC_TLS_DTPMOD32 +0+ +sg1 \+ 0
0+12168 +0+154c R_SPARC_TLS_DTPOFF32 +0+ +sg1 \+ 0
0+12174 +0+184e R_SPARC_TLS_TPOFF32 +0+4 +sg2 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 Offset +Info +Type +Sym. Value +Symbol's Name \+ Addend
0+12128 +0+1415 R_SPARC_JMP_SLOT +0+ +__tls_get_addr \+ 0

Symbol table '.dynsym' contains 30 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND *
 +1: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +1 *
 +2: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +2 *
 +3: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +3 *
 +4: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +4 *
 +5: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +5 *
 +6: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +6 *
 +7: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +7 *
 +8: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +8 *
 +9: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +9 *
 +10: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +10 *
 +11: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +11 *
 +12: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +12 *
 +13: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +13 *
 +14: 0+1c +0 TLS +GLOBAL DEFAULT +8 sg8
 +15: 0+12060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +16: 0+8 +0 TLS +GLOBAL DEFAULT +8 sg3
 +17: 0+c +0 TLS +GLOBAL DEFAULT +8 sg4
 +18: 0+10 +0 TLS +GLOBAL DEFAULT +8 sg5
 +19: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +20: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
 +21: 0+ +0 TLS +GLOBAL DEFAULT +8 sg1
 +22: 0+1008 +0 FUNC +GLOBAL DEFAULT +6 fn1
 +23: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +24: 0+4 +0 TLS +GLOBAL DEFAULT +8 sg2
 +25: 0+14 +0 TLS +GLOBAL DEFAULT +8 sg6
 +26: 0+18 +0 TLS +GLOBAL DEFAULT +8 sg7
 +27: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +28: 0+12138 +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +29: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 57 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND *
 +1: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +1 *
 +2: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +2 *
 +3: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +3 *
 +4: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +4 *
 +5: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +5 *
 +6: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +6 *
 +7: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +7 *
 +8: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +8 *
 +9: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +9 *
 +10: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +10 *
 +11: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +11 *
 +12: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +12 *
 +13: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +13 *
 +14: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +14 *
 +15: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +15 *
 +16: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +16 *
 +17: 0+20 +0 TLS +LOCAL +DEFAULT +8 sl1
 +18: 0+24 +0 TLS +LOCAL +DEFAULT +8 sl2
 +19: 0+28 +0 TLS +LOCAL +DEFAULT +8 sl3
 +20: 0+2c +0 TLS +LOCAL +DEFAULT +8 sl4
 +21: 0+30 +0 TLS +LOCAL +DEFAULT +8 sl5
 +22: 0+34 +0 TLS +LOCAL +DEFAULT +8 sl6
 +23: 0+38 +0 TLS +LOCAL +DEFAULT +8 sl7
 +24: 0+3c +0 TLS +LOCAL +DEFAULT +8 sl8
 +25: 0+60 +0 TLS +LOCAL +HIDDEN +9 sH1
 +26: 0+48 +0 TLS +LOCAL +HIDDEN +8 sh3
 +27: 0+64 +0 TLS +LOCAL +HIDDEN +9 sH2
 +28: 0+78 +0 TLS +LOCAL +HIDDEN +9 sH7
 +29: 0+58 +0 TLS +LOCAL +HIDDEN +8 sh7
 +30: 0+5c +0 TLS +LOCAL +HIDDEN +8 sh8
 +31: 0+6c +0 TLS +LOCAL +HIDDEN +9 sH4
 +32: 0+4c +0 TLS +LOCAL +HIDDEN +8 sh4
 +33: 0+68 +0 TLS +LOCAL +HIDDEN +9 sH3
 +34: 0+50 +0 TLS +LOCAL +HIDDEN +8 sh5
 +35: 0+70 +0 TLS +LOCAL +HIDDEN +9 sH5
 +36: 0+74 +0 TLS +LOCAL +HIDDEN +9 sH6
 +37: 0+7c +0 TLS +LOCAL +HIDDEN +9 sH8
 +38: 0+40 +0 TLS +LOCAL +HIDDEN +8 sh1
 +39: 0+44 +0 TLS +LOCAL +HIDDEN +8 sh2
 +40: 0+54 +0 TLS +LOCAL +HIDDEN +8 sh6
 +41: 0+1c +0 TLS +GLOBAL DEFAULT +8 sg8
 +42: 0+12060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +43: 0+8 +0 TLS +GLOBAL DEFAULT +8 sg3
 +44: 0+c +0 TLS +GLOBAL DEFAULT +8 sg4
 +45: 0+10 +0 TLS +GLOBAL DEFAULT +8 sg5
 +46: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +47: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
 +48: 0+ +0 TLS +GLOBAL DEFAULT +8 sg1
 +49: 0+1008 +0 FUNC +GLOBAL DEFAULT +6 fn1
 +50: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +51: 0+4 +0 TLS +GLOBAL DEFAULT +8 sg2
 +52: 0+14 +0 TLS +GLOBAL DEFAULT +8 sg6
 +53: 0+18 +0 TLS +GLOBAL DEFAULT +8 sg7
 +54: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +55: 0+12138 +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +56: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
