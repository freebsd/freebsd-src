#source: tlssunnopic32.s
#source: tlsnopic.s
#as: --32
#ld: -shared -melf32_sparc
#readelf: -WSsrl
#target: sparc-*-*

There are 15 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
 +\[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] .hash +.*
 +\[ 2\] .dynsym +.*
 +\[ 3\] .dynstr +.*
 +\[ 4\] .rela.dyn +.*
 +\[ 5\] .text +PROGBITS +0+1000 0+1000 0+1000 0+ +AX +0 +0 4096
 +\[ 6\] .data +PROGBITS +0+12000 0+2000 0+ 0+ +WA +0 +0 4096
 +\[ 7\] .tbss +NOBITS +0+12000 0+2000 0+24 0+ WAT +0 +0 +4
 +\[ 8\] .dynamic +DYNAMIC +0+12000 0+2000 0+80 08 +WA +3 +0 +4
 +\[ 9\] .plt +.*
 +\[10\] .got +PROGBITS +0+12080 0+2080 0+1c 04 +WA +0 +0 +4
 +\[11\] .bss +.*
 +\[12\] .shstrtab +.*
 +\[13\] .symtab +.*
 +\[14\] .strtab +.*
#...
Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9a-f]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x0+2000 0x0+2000 R E 0x10000
 +LOAD +0x0+2000 0x0+12000 0x0+12000 0x0+9c 0x0+a0 RWE 0x10000
 +DYNAMIC +0x0+2000 0x0+12000 0x0+12000 0x0+80 0x0+80 RW +0x4
 +TLS +0x0+2000 0x0+12000 0x0+12000 0x0+ 0x0+24 R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 12 entries:
 Offset +Info +Type +Sym. Value +Symbol's Name \+ Addend
0+1004 +0+a09 R_SPARC_HI22 +0+12080 +\.got \+ 12080
0+1008 +0+a0c R_SPARC_LO10 +0+12080 +\.got \+ 12080
0+10dc +0+48 R_SPARC_TLS_LE_HIX22 +0+9
0+10e0 +0+49 R_SPARC_TLS_LE_LOX10 +0+9
0+10f8 +0+48 R_SPARC_TLS_LE_HIX22 +0+1c
0+10fc +0+49 R_SPARC_TLS_LE_LOX10 +0+1c
0+12084 +0+4e R_SPARC_TLS_TPOFF32 +0+
0+12088 +0+4e R_SPARC_TLS_TPOFF32 +0+4
0+12094 +0+4e R_SPARC_TLS_TPOFF32 +0+14
0+12098 +0+4e R_SPARC_TLS_TPOFF32 +0+18
0+1208c +0+f4e R_SPARC_TLS_TPOFF32 +0+ +sg1 \+ 0
0+12090 +0+114e R_SPARC_TLS_TPOFF32 +0+ +sg2 \+ 0

Symbol table '.dynsym' contains 20 entries:
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
 +12: 0+12000 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +13: 0+1000 +0 FUNC +GLOBAL DEFAULT +5 fn3
 +14: 0+12080 +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +15: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND sg1
 +16: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +17: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND sg2
 +18: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +19: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 33 entries:
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
 +15: 0+ +0 TLS +LOCAL +DEFAULT +7 bl1
 +16: 0+4 +0 TLS +LOCAL +DEFAULT +7 bl2
 +17: 0+8 +0 TLS +LOCAL +DEFAULT +7 bl3
 +18: 0+c +0 TLS +LOCAL +DEFAULT +7 bl4
 +19: 0+10 +0 TLS +LOCAL +DEFAULT +7 bl5
 +20: 0+1c +0 TLS +LOCAL +HIDDEN +7 sh3
 +21: 0+20 +0 TLS +LOCAL +HIDDEN +7 sh4
 +22: 0+14 +0 TLS +LOCAL +HIDDEN +7 sh1
 +23: 0+12080 +0 OBJECT +LOCAL +HIDDEN +ABS _GLOBAL_OFFSET_TABLE_
 +24: 0+18 +0 TLS +LOCAL +HIDDEN +7 sh2
 +25: 0+12000 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +26: 0+1000 +0 FUNC +GLOBAL DEFAULT +5 fn3
 +27: 0+12080 +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +28: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND sg1
 +29: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +30: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND sg2
 +31: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +32: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
