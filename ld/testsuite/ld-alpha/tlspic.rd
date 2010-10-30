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
 +\[ 7\] .eh_frame +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 00 +A +0 +0 +8
 +\[ 8\] .tdata +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ WAT +0 +0 +4
 +\[ 9\] .tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ WAT +0 +0 +1
 +\[10\] .dynamic +DYNAMIC +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 10 +WA +3 +0 +8
 +\[11\] .plt +.*
 +\[12\] .got +PROGBITS +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 0+ +WA +0 +0 +8
 +\[13\] .shstrtab +.*
 +\[14\] .symtab +.*
 +\[15\] .strtab +.*
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9a-f]+

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x10000
 +LOAD +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RWE 0x10000
 +DYNAMIC +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ RW +0x8
 +TLS +0x0+10e0 0x0+110e0 0x0+110e0 0x0+60 0x0+80 R +0x4
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
.* [0-9a-f]+     0 NOTYPE  LOCAL  DEFAULT  UND 
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg8
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg3
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg4
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg5
.* [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg1
.* [0-9a-f]+   172 FUNC    GLOBAL DEFAULT \[<other>: 88\]     6 fn1
.* [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg2
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg6
.* [0-9a-f]+     0 TLS     GLOBAL DEFAULT    8 sg7
.* [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  ABS _edata
.* [0-9a-f]+     0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* [0-9a-f]+ +0 NOTYPE +LOCAL +DEFAULT +UND 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +1 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +2 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +3 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +4 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +5 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +6 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +7 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +8 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +9 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +10 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +11 
.* [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +12 
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl1
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl2
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl3
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl4
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl5
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl6
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl7
.* [0-9a-f]+ +0 TLS +LOCAL +DEFAULT +8 sl8
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH1
.* [0-9a-f]+ +0 OBJECT +LOCAL +HIDDEN +ABS _DYNAMIC
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh3
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH2
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH7
.* [0-9a-f]+ +0 OBJECT +LOCAL +HIDDEN +ABS _PROCEDURE_LINKAGE_TABLE_
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh7
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh8
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH4
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh4
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH3
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh5
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH5
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH6
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +9 sH8
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh1
.* [0-9a-f]+ +0 OBJECT +LOCAL +HIDDEN +ABS _GLOBAL_OFFSET_TABLE_
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh2
.* [0-9a-f]+ +0 TLS +LOCAL +HIDDEN +8 sh6
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg8
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg3
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg4
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg5
.* [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg1
.* [0-9a-f]+ +172 FUNC +GLOBAL DEFAULT +\[<other>: 88\] +6 fn1
.* [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg2
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg6
.* [0-9a-f]+ +0 TLS +GLOBAL DEFAULT +8 sg7
.* [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
.* [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
