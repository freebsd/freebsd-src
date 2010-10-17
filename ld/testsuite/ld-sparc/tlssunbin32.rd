#source: tlssunbin32.s
#as: --32
#ld: -shared -melf32_sparc tmpdir/libtlslib32.so tmpdir/tlssunbinpic32.o
#readelf: -WSsrl
#target: sparc*-*-*

There are 17 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
 +\[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
 +\[ 1\] .interp +.*
 +\[ 2\] .hash +.*
 +\[ 3\] .dynsym +.*
 +\[ 4\] .dynstr +.*
 +\[ 5\] .rela.dyn +.*
 +\[ 6\] .text +PROGBITS +0+11000 0+1000 0+1194 00 +AX +0 +0 4096
 +\[ 7\] .data +.*
 +\[ 8\] .tdata +PROGBITS +0+24000 0+4000 0+1060 00 WAT +0 +0 +4
 +\[ 9\] .tbss +NOBITS +0+25060 0+5060 0+40 00 WAT +0 +0 +4
 +\[10\] .dynamic +DYNAMIC +0+25060 0+5060 0+80 08 +WA +4 +0 +4
 +\[11\] .plt +.*
 +\[12\] .got +PROGBITS +0+250e0 0+50e0 0+14 04 +WA +0 +0 +4
 +\[13\] .bss +.*
 +\[14\] .shstrtab +.*
 +\[15\] .symtab +.*
 +\[16\] .strtab +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x12000
There are 6 program headers, starting at offset [0-9]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz MemSiz +Flg Align
 +PHDR +0x0+34 0x0+10034 0x0+10034 0x0+c0 0x0+c0 R E 0x4
 +INTERP +0x0+f4 0x0+100f4 0x0+100f4 0x0+11 0x0+11 R +0x1
.*Requesting program interpreter.*
 +LOAD +0x0+ 0x0+10000 0x0+10000 0x0+2194 0x0+2194 R E 0x10000
 +LOAD +0x0+4000 0x0+24000 0x0+24000 0x0+10f4 0x0+10f8 RWE 0x10000
 +DYNAMIC +0x0+5060 0x0+25060 0x0+25060 0x0+80 0x0+80 RW +0x4
 +TLS +0x0+4000 0x0+24000 0x0+24000 0x0+1060 0x0+10a0 R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 4 entries:
 Offset +Info +Type +Sym. Value +Symbol's Name \+ Addend
000250e4 +0000014e R_SPARC_TLS_TPOFF32 +00000000 +sG5 \+ 0
000250e8 +0000034e R_SPARC_TLS_TPOFF32 +00000000 +sG2 \+ 0
000250ec +0000074e R_SPARC_TLS_TPOFF32 +00000000 +sG6 \+ 0
000250f0 +0000084e R_SPARC_TLS_TPOFF32 +00000000 +sG1 \+ 0

Symbol table '.dynsym' contains 11 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND *
 +1: 0+ +0 TLS +GLOBAL DEFAULT +UND sG5
 +2: 0+25060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +3: 0+ +0 TLS +GLOBAL DEFAULT +UND sG2
 +4: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +5: 0+ +0 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
 +6: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +7: 0+ +0 TLS +GLOBAL DEFAULT +UND sG6
 +8: 0+ +0 TLS +GLOBAL DEFAULT +UND sG1
 +9: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +10: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 70 entries:
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
 +17: 0+1020 +0 TLS +LOCAL +DEFAULT +8 sl1
 +18: 0+1024 +0 TLS +LOCAL +DEFAULT +8 sl2
 +19: 0+1028 +0 TLS +LOCAL +DEFAULT +8 sl3
 +20: 0+102c +0 TLS +LOCAL +DEFAULT +8 sl4
 +21: 0+1030 +0 TLS +LOCAL +DEFAULT +8 sl5
 +22: 0+1034 +0 TLS +LOCAL +DEFAULT +8 sl6
 +23: 0+1038 +0 TLS +LOCAL +DEFAULT +8 sl7
 +24: 0+103c +0 TLS +LOCAL +DEFAULT +8 sl8
 +25: 0+1080 +0 TLS +LOCAL +DEFAULT +9 bl1
 +26: 0+1084 +0 TLS +LOCAL +DEFAULT +9 bl2
 +27: 0+1088 +0 TLS +LOCAL +DEFAULT +9 bl3
 +28: 0+108c +0 TLS +LOCAL +DEFAULT +9 bl4
 +29: 0+1090 +0 TLS +LOCAL +DEFAULT +9 bl5
 +30: 0+1094 +0 TLS +LOCAL +DEFAULT +9 bl6
 +31: 0+1098 +0 TLS +LOCAL +DEFAULT +9 bl7
 +32: 0+109c +0 TLS +LOCAL +DEFAULT +9 bl8
 +33: 0+250e0 +0 OBJECT +LOCAL +HIDDEN +12 _GLOBAL_OFFSET_TABLE_
 +34: 0+101c +0 TLS +GLOBAL DEFAULT +8 sg8
 +35: 0+107c +0 TLS +GLOBAL DEFAULT +9 bg8
 +36: 0+1074 +0 TLS +GLOBAL DEFAULT +9 bg6
 +37: 0+ +0 TLS +GLOBAL DEFAULT +UND sG5
 +38: 0+1068 +0 TLS +GLOBAL DEFAULT +9 bg3
 +39: 0+25060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +40: 0+1008 +0 TLS +GLOBAL DEFAULT +8 sg3
 +41: 0+1048 +0 TLS +GLOBAL HIDDEN +8 sh3
 +42: 0+ +0 TLS +GLOBAL DEFAULT +UND sG2
 +43: 0+100c +0 TLS +GLOBAL DEFAULT +8 sg4
 +44: 0+1010 +0 TLS +GLOBAL DEFAULT +8 sg5
 +45: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +46: 0+1070 +0 TLS +GLOBAL DEFAULT +9 bg5
 +47: 0+ +0 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
 +48: 0+1058 +0 TLS +GLOBAL HIDDEN +8 sh7
 +49: 0+105c +0 TLS +GLOBAL HIDDEN +8 sh8
 +50: 0+ +0 TLS +GLOBAL DEFAULT +8 sg1
 +51: 0+12000 +0 FUNC +GLOBAL DEFAULT +6 _start
 +52: 0+104c +0 TLS +GLOBAL HIDDEN +8 sh4
 +53: 0+1078 +0 TLS +GLOBAL DEFAULT +9 bg7
 +54: 0+1050 +0 TLS +GLOBAL HIDDEN +8 sh5
 +55: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +56: 0+ +0 TLS +GLOBAL DEFAULT +UND sG6
 +57: 0+11008 +0 FUNC +GLOBAL DEFAULT +6 fn2
 +58: 0+1004 +0 TLS +GLOBAL DEFAULT +8 sg2
 +59: 0+ +0 TLS +GLOBAL DEFAULT +UND sG1
 +60: 0+1040 +0 TLS +GLOBAL HIDDEN +8 sh1
 +61: 0+1014 +0 TLS +GLOBAL DEFAULT +8 sg6
 +62: 0+1018 +0 TLS +GLOBAL DEFAULT +8 sg7
 +63: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +64: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +65: 0+1044 +0 TLS +GLOBAL HIDDEN +8 sh2
 +66: 0+1054 +0 TLS +GLOBAL HIDDEN +8 sh6
 +67: 0+1064 +0 TLS +GLOBAL DEFAULT +9 bg2
 +68: 0+1060 +0 TLS +GLOBAL DEFAULT +9 bg1
 +69: 0+106c +0 TLS +GLOBAL DEFAULT +9 bg4
