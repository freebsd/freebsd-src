#source: tlstoc.s
#as: -a64
#ld: -shared -melf64ppc
#readelf: -WSsrl
#target: powerpc64*-*-*

There are 16 section headers.*

Section Headers:
 +\[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] \.hash .*
 +\[ 2\] \.dynsym .*
 +\[ 3\] \.dynstr .*
 +\[ 4\] \.rela\.dyn .*
 +\[ 5\] \.rela\.plt .*
 +\[ 6\] \.text .*
 +\[ 7\] \.tdata +PROGBITS .* 0+38 0+ WAT +0 +0 +8
 +\[ 8\] \.tbss +NOBITS .* 0+38 0+ WAT +0 +0 +8
 +\[ 9\] \.data\.rel\.ro .*
 +\[10\] \.dynamic .*
 +\[11\] \.got .*
 +\[12\] \.plt .*
 +\[13\] \.shstrtab .*
 +\[14\] \.symtab .*
 +\[15\] \.strtab .*
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x[0-9a-f]+
There are 4 program headers.*

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +LOAD .* R E 0x10000
 +LOAD .* RW +0x10000
 +DYNAMIC .* RW +0x8
 +TLS .* 0x0+38 0x0+70 R +0x8

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +0+ +\.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.text 
 +01 +\.tdata \.dynamic \.got \.plt 
 +02 +\.dynamic 
 +03 +\.tdata \.tbss 

Relocation section '\.rela\.dyn' at offset .* contains 11 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC64_TPREL16 +0+60 le0 \+ 0
[0-9a-f ]+R_PPC64_TPREL16_HA +0+68 le1 \+ 0
[0-9a-f ]+R_PPC64_TPREL16_LO +0+68 le1 \+ 0
[0-9a-f ]+R_PPC64_DTPMOD64 +0+ gd \+ 0
[0-9a-f ]+R_PPC64_DTPREL64 +0+ gd \+ 0
[0-9a-f ]+R_PPC64_DTPMOD64 +0+ ld \+ 0
[0-9a-f ]+R_PPC64_DTPMOD64 +0+38 gd0 \+ 0
[0-9a-f ]+R_PPC64_DTPREL64 +0+38 gd0 \+ 0
[0-9a-f ]+R_PPC64_DTPMOD64 +0+40 ld0 \+ 0
[0-9a-f ]+R_PPC64_DTPREL64 +0+50 ld2 \+ 0
[0-9a-f ]+R_PPC64_TPREL64 +0+58 ie0 \+ 0

Relocation section '\.rela\.plt' at offset .* contains 1 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC64_JMP_SLOT +0+ __tls_get_addr \+ 0

Symbol table '\.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND 
.* SECTION LOCAL +DEFAULT +6 
.* SECTION LOCAL +DEFAULT +7 
.* SECTION LOCAL +DEFAULT +8 
.* SECTION LOCAL +DEFAULT +9 
.* TLS +GLOBAL DEFAULT +UND gd
.* TLS +GLOBAL DEFAULT +8 le0
.* NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +8 ld0
.* TLS +GLOBAL DEFAULT +8 le1
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +6 _start
.* TLS +GLOBAL DEFAULT +8 ld2
.* TLS +GLOBAL DEFAULT +8 ld1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
.* TLS +GLOBAL DEFAULT +8 gd0
.* TLS +GLOBAL DEFAULT +8 ie0

Symbol table '\.symtab' contains 40 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND 
.* SECTION LOCAL +DEFAULT +1 
.* SECTION LOCAL +DEFAULT +2 
.* SECTION LOCAL +DEFAULT +3 
.* SECTION LOCAL +DEFAULT +4 
.* SECTION LOCAL +DEFAULT +5 
.* SECTION LOCAL +DEFAULT +6 
.* SECTION LOCAL +DEFAULT +7 
.* SECTION LOCAL +DEFAULT +8 
.* SECTION LOCAL +DEFAULT +9 
.* SECTION LOCAL +DEFAULT +10 
.* SECTION LOCAL +DEFAULT +11 
.* SECTION LOCAL +DEFAULT +12 
.* SECTION LOCAL +DEFAULT +13 
.* SECTION LOCAL +DEFAULT +14 
.* SECTION LOCAL +DEFAULT +15 
.* TLS +LOCAL +DEFAULT +7 gd4
.* TLS +LOCAL +DEFAULT +7 ld4
.* TLS +LOCAL +DEFAULT +7 ld5
.* TLS +LOCAL +DEFAULT +7 ld6
.* TLS +LOCAL +DEFAULT +7 ie4
.* TLS +LOCAL +DEFAULT +7 le4
.* TLS +LOCAL +DEFAULT +7 le5
.* NOTYPE +LOCAL +DEFAULT +11 \.Lie0
.* OBJECT +LOCAL +HIDDEN +ABS _DYNAMIC
.* NOTYPE +LOCAL +DEFAULT +6 \.__tls_get_addr
.* TLS +GLOBAL DEFAULT +UND gd
.* TLS +GLOBAL DEFAULT +8 le0
.* NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +8 ld0
.* TLS +GLOBAL DEFAULT +8 le1
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +6 _start
.* TLS +GLOBAL DEFAULT +8 ld2
.* TLS +GLOBAL DEFAULT +8 ld1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
.* TLS +GLOBAL DEFAULT +8 gd0
.* TLS +GLOBAL DEFAULT +8 ie0
