#source: align.s
#source: tlsbinpic.s
#source: tlsbin.s
#as:
#ld: -relax -melf64alpha
#readelf: -WSsrl
#target: alpha*-*-*

There are [0-9]+ section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
 +\[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] .interp +.*
 +\[ 2\] .hash +.*
 +\[ 3\] .dynsym +.*
 +\[ 4\] .dynstr +.*
 +\[ 5\] .rela.dyn +.*
 +\[ 6\] .rela.plt +.*
 +\[ 7\] .text +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ +AX +0 +0 4096
 +\[ 8\] .data +.*
 +\[ 9\] .tdata +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ WAT +0 +0 +4
 +\[10\] .tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ WAT +0 +0 +1
 +\[11\] .eh_frame +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 +A +0 +0 +8
 +\[12\] .dynamic +DYNAMIC +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 10 +WA +4 +0 +8
 +\[13\] .plt +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ WAX +0 +0 +8
 +\[14\] .got +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ +WA +0 +0 +8
 +\[15\] .sbss +.*
 +\[16\] .bss +.*
 +\[17\] .shstrtab +.*
 +\[18\] .symtab +.*
 +\[19\] .strtab +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x[0-9a-f]+
There are 6 program headers, starting at offset [0-9]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +PHDR +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x8
 +INTERP +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ R +0x1
.*Requesting program interpreter.*
 +LOAD +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x10000
 +LOAD +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RWE 0x10000
 +DYNAMIC +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RW +0x8
 +TLS +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 2 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f]+ +0+200000026 R_ALPHA_TPREL64 +0+ sG2 \+ 0
[0-9a-f]+ +0+600000026 R_ALPHA_TPREL64 +0+ sG1 \+ 0

Symbol table '.dynsym' contains 10 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +2: 0+ +0 TLS +GLOBAL DEFAULT +UND sG2
 +3: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +4: 0+ +4 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
 +5: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +6: 0+ +0 TLS +GLOBAL DEFAULT +UND sG1
 +7: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +8: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +9: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 71 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: [0-9a-f]+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +1 
 +2: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +2 
 +3: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +3 
 +4: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +4 
 +5: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +5 
 +6: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +6 
 +7: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +7 
 +8: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +8 
 +9: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +9 
 +10: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +10 
 +11: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +11 
 +12: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +12 
 +13: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +13 
 +14: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +14 
 +15: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +15 
 +16: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +16 
 +17: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +17 
 +18: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +18 
 +19: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +19 
 +20: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl1
 +21: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl2
 +22: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl3
 +23: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl4
 +24: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl5
 +25: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl6
 +26: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl7
 +27: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +9 sl8
 +28: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl1
 +29: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl2
 +30: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl3
 +31: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl4
 +32: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl5
 +33: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl6
 +34: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl7
 +35: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +10 bl8
 +36: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg8
 +37: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg8
 +38: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg6
 +39: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg3
 +40: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +41: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg3
 +42: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh3
 +43: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +UND sG2
 +44: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg4
 +45: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg5
 +46: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +47: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg5
 +48: [0-9a-f]+ +4 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
 +49: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh7
 +50: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh8
 +51: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg1
 +52: [0-9a-f]+ +52 FUNC +GLOBAL DEFAULT +7 _start
 +53: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh4
 +54: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg7
 +55: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh5
 +56: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +57: [0-9a-f]+ +136 FUNC +GLOBAL DEFAULT +7 fn2
 +58: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg2
 +59: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +UND sG1
 +60: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh1
 +61: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg6
 +62: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +9 sg7
 +63: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +64: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +65: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +66: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh2
 +67: [0-9a-f]+ +0 TLS +GLOBAL HIDDEN +9 sh6
 +68: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg2
 +69: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg1
 +70: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +10 bg4
