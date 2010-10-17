#source: align.s
#source: tlspic1.s
#source: tlspic2.s
#as:
#ld: -shared -melf64alpha
#readelf: -WSsrl
#target: alpha*-*-*

There are [0-9]* section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
 +\[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] .hash +.*
 +\[ 2\] .dynsym +.*
 +\[ 3\] .dynstr +.*
 +\[ 4\] .rela.dyn +.*
 +\[ 5\] .rela.plt +.*
 +\[ 6\] .text +PROGBITS +0+1000 0+1000 0+ac 0+ +AX +0 +0 4096
 +\[ 7\] .data +.*
 +\[ 8\] .tdata +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ WAT +0 +0 +4
 +\[ 9\] .tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ WAT +0 +0 +1
 +\[10\] .eh_frame +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 +A +0 +0 +8
 +\[11\] .dynamic +DYNAMIC +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 10 +WA +3 +0 +8
 +\[12\] .plt +.*
 +\[13\] .got +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ +WA +0 +0 +8
 +\[14\] .sbss +.*
 +\[15\] .bss +.*
 +\[16\] .shstrtab +.*
 +\[17\] .symtab +.*
 +\[18\] .strtab +.*
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9a-f]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x10000
 +LOAD +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RWE 0x10000
 +DYNAMIC +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RW +0x8
 +TLS +0x0+2000 0x0+12000 0x0+12000 0x0+60 0x0+80 R +0x4
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 7 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_DTPMOD64 +0+ sg1 \+ 0
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_DTPREL64 +0+ sg1 \+ 0
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_TPREL64 +0+4 sg2 \+ 0
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_TPREL64 +0+44
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_DTPMOD64 +0+
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_DTPMOD64 +0+
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_TPREL64 +0+24

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f]+ +[0-9a-f]+ R_ALPHA_JMP_SLOT +0+ __tls_get_addr \+ 0

Symbol table '.dynsym' contains [0-9]+ entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: [0-9a-f]+     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    1 
     2: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    2 
     3: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    3 
     4: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    4 
     5: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    5 
     6: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    6 
     7: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    7 
     8: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    8 
     9: [0-9a-f]+     0 SECTION LOCAL  DEFAULT    9 
    10: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   10 
    11: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   11 
    12: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   12 
    13: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   13 
    14: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   14 
    15: [0-9a-f]+     0 SECTION LOCAL  DEFAULT   15 
    16: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg8
    17: [0-9a-f]+     0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
    18: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg3
    19: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg4
    20: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg5
    21: [0-9a-f]+     0 OBJECT  GLOBAL DEFAULT  ABS _PROCEDURE_LINKAGE_TABLE_
    22: [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
    23: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg1
    24: [0-9a-f]+   172 FUNC    GLOBAL DEFAULT    6 fn1
    25: [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
    26: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg2
    27: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg6
    28: [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg7
    29: [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  ABS _edata
    30: [0-9a-f]+     0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
    31: [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains [0-9]+ entries:
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
 +19: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl1
 +20: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl2
 +21: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl3
 +22: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl4
 +23: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl5
 +24: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl6
 +25: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl7
 +26: [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl8
 +27: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH1
 +28: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh3
 +29: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH2
 +30: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH7
 +31: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh7
 +32: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh8
 +33: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH4
 +34: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh4
 +35: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH3
 +36: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh5
 +37: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH5
 +38: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH6
 +39: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH8
 +40: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh1
 +41: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh2
 +42: [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh6
 +43: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg8
 +44: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +45: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg3
 +46: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg4
 +47: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg5
 +48: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _PROCEDURE_LINKAGE_TABLE_
 +49: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
 +50: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg1
 +51: [0-9a-f]+ +172 FUNC +GLOBAL DEFAULT +6 fn1
 +52: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +53: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg2
 +54: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg6
 +55: [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg7
 +56: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +57: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +58: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
