#source: tlslib.s
#source: tlstoc.s
#as: -a64
#ld: -melf64ppc
#readelf: -WSsrl
#target: powerpc64*-*-*

There are 17 section headers.*

Section Headers:
 +\[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] \.interp +.*
 +\[ 2\] \.hash +.*
 +\[ 3\] \.dynsym +.*
 +\[ 4\] \.dynstr +.*
 +\[ 5\] \.rela\.dyn +.*
 +\[ 6\] \.rela\.plt +.*
 +\[ 7\] \.text +PROGBITS .* 0+bc 0+ +AX +0 +0 +4
 +\[ 8\] \.rodata +PROGBITS .* 0+ 0+ +A +0 +0 +8
 +\[ 9\] \.tdata +PROGBITS .* 0+38 0+ WAT +0 +0 +8
 +\[10\] \.tbss +NOBITS .* 0+38 0+ WAT +0 +0 +8
 +\[11\] \.dynamic +DYNAMIC .* 0+150 10 +WA +4 +0 +8
 +\[12\] \.got +PROGBITS .* 0+58 08 +WA +0 +0 +8
 +\[13\] \.plt +.*
 +\[14\] \.shstrtab +.*
 +\[15\] \.symtab +.*
 +\[16\] \.strtab +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point .*
There are 6 program headers.*

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +PHDR +0x0+40 0x0+10000040 0x0+10000040 0x0+150 0x0+150 R E 0x8
 +INTERP +0x0+190 0x0+10000190 0x0+10000190 0x0+11 0x0+11 R +0x1
 +\[Requesting program interpreter: .*\]
 +LOAD .* R E 0x10000
 +LOAD .* RW +0x10000
 +DYNAMIC .* RW +0x8
 +TLS .* 0x0+38 0x0+70 R +0x8

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +0+ +
 +01 +\.interp 
 +02 +\.interp \.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.text 
 +03 +\.tdata \.dynamic \.got \.plt 
 +04 +\.dynamic 
 +05 +\.tdata \.tbss 

Relocation section '\.rela\.dyn' at offset .* contains 3 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC64_DTPMOD64 +0+ gd \+ 0
[0-9a-f ]+R_PPC64_DTPREL64 +0+ gd \+ 0
[0-9a-f ]+R_PPC64_DTPMOD64 +0+ ld \+ 0

Relocation section '\.rela\.plt' at offset .* contains 1 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC64_JMP_SLOT +0+ __tls_get_addr \+ 0

Symbol table '\.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND 
.* TLS +GLOBAL DEFAULT +UND gd
.* FUNC +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '\.symtab' contains 41 entries:
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
.* SECTION LOCAL +DEFAULT +16 
.* TLS +LOCAL +DEFAULT +9 gd4
.* TLS +LOCAL +DEFAULT +9 ld4
.* TLS +LOCAL +DEFAULT +9 ld5
.* TLS +LOCAL +DEFAULT +9 ld6
.* TLS +LOCAL +DEFAULT +9 ie4
.* TLS +LOCAL +DEFAULT +9 le4
.* TLS +LOCAL +DEFAULT +9 le5
.* NOTYPE +LOCAL +DEFAULT +12 \.Lie0
.* OBJECT +LOCAL +HIDDEN +11 _DYNAMIC
.* FUNC +LOCAL +DEFAULT +UND \.__tls_get_addr
.* TLS +GLOBAL DEFAULT +UND gd
.* TLS +GLOBAL DEFAULT +10 le0
.* FUNC +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +10 ld0
.* TLS +GLOBAL DEFAULT +10 le1
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +7 _start
.* TLS +GLOBAL DEFAULT +10 ld2
.* TLS +GLOBAL DEFAULT +10 ld1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
.* TLS +GLOBAL DEFAULT +10 gd0
.* TLS +GLOBAL DEFAULT +10 ie0
