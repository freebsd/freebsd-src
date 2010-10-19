#source: tls32.s
#as: -a32
#ld: -shared -melf32ppc
#readelf: -WSsrl
#target: powerpc*-*-*

There are 15 section headers.*

Section Headers:
 +\[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] \.hash +.*
 +\[ 2\] \.dynsym +.*
 +\[ 3\] \.dynstr +.*
 +\[ 4\] \.rela\.dyn +.*
 +\[ 5\] \.rela\.plt +.*
 +\[ 6\] \.text +PROGBITS .* 0+70 0+ +AX +0 +0 +1
 +\[ 7\] \.tdata +PROGBITS .* 0+1c 0+ WAT +0 +0 +4
 +\[ 8\] \.tbss +NOBITS .* 0+1c 0+ WAT +0 +0 +4
 +\[ 9\] \.dynamic +DYNAMIC .* 08 +WA +3 +0 +4
 +\[10\] \.got +PROGBITS .* 0+34 04 WAX +0 +0 +4
 +\[11\] \.plt +.*
 +\[12\] \.shstrtab +.*
 +\[13\] \.symtab +.*
 +\[14\] \.strtab +.*
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x[0-9a-f]+
There are 4 program headers.*

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz MemSiz +Flg Align
 +LOAD .* R E 0x10000
 +LOAD .* RWE 0x10000
 +DYNAMIC .* RW +0x4
 +TLS .* 0x0+1c 0x0+38 R +0x4

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +0+ +\.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.text 
 +01 +\.tdata \.dynamic \.got \.plt 
 +02 +\.dynamic 
 +03 +\.tdata \.tbss 

Relocation section '\.rela\.dyn' at offset 0x[0-9a-f]+ contains 18 entries:
 Offset +Info +Type +Sym\. Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC_REL24 +0+ +__tls_get_addr \+ 0
[0-9a-f ]+R_PPC_REL24 +0+ +__tls_get_addr \+ 0
[0-9a-f ]+R_PPC_REL24 +0+ +__tls_get_addr \+ 0
[0-9a-f ]+R_PPC_REL24 +0+ +__tls_get_addr \+ 0
[0-9a-f ]+R_PPC_TPREL16 +0+30 +le0 \+ 0
[0-9a-f ]+R_PPC_TPREL16_HA +0+34 +le1 \+ 0
[0-9a-f ]+R_PPC_TPREL16_LO +0+34 +le1 \+ 0
[0-9a-f ]+R_PPC_TPREL16 +0+1041c +\.tdata \+ 10430
[0-9a-f ]+R_PPC_TPREL16_HA +0+1041c +\.tdata \+ 10434
[0-9a-f ]+R_PPC_TPREL16_LO +0+1041c +\.tdata \+ 10434
[0-9a-f ]+R_PPC_DTPMOD32 +0+
[0-9a-f ]+R_PPC_DTPREL32 +0+
[0-9a-f ]+R_PPC_DTPMOD32 +0+
[0-9a-f ]+R_PPC_DTPMOD32 +0+ +gd \+ 0
[0-9a-f ]+R_PPC_DTPREL32 +0+ +gd \+ 0
[0-9a-f ]+R_PPC_DTPMOD32 +0+1c +gd0 \+ 0
[0-9a-f ]+R_PPC_DTPREL32 +0+1c +gd0 \+ 0
[0-9a-f ]+R_PPC_TPREL32 +0+2c +ie0 \+ 0

Relocation section '\.rela\.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 Offset +Info +Type +Sym\. Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC_JMP_SLOT +0+ +__tls_get_addr \+ 0

Symbol table '\.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND 
.* SECTION LOCAL +DEFAULT +6 
.* SECTION LOCAL +DEFAULT +7 
.* SECTION LOCAL +DEFAULT +8 
.* TLS +GLOBAL DEFAULT +UND gd
.* TLS +GLOBAL DEFAULT +8 le0
.* NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +8 ld0
.* TLS +GLOBAL DEFAULT +8 le1
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +6 _start
.* NOTYPE +GLOBAL DEFAULT +ABS __end
.* TLS +GLOBAL DEFAULT +8 ld2
.* TLS +GLOBAL DEFAULT +8 ld1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
.* TLS +GLOBAL DEFAULT +8 gd0
.* TLS +GLOBAL DEFAULT +8 ie0

Symbol table '\.symtab' contains 39 entries:
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
.* TLS +LOCAL +DEFAULT +7 gd4
.* TLS +LOCAL +DEFAULT +7 ld4
.* TLS +LOCAL +DEFAULT +7 ld5
.* TLS +LOCAL +DEFAULT +7 ld6
.* TLS +LOCAL +DEFAULT +7 ie4
.* TLS +LOCAL +DEFAULT +7 le4
.* TLS +LOCAL +DEFAULT +7 le5
.* OBJECT +LOCAL +HIDDEN +ABS _DYNAMIC
.* OBJECT +LOCAL +HIDDEN +ABS _GLOBAL_OFFSET_TABLE_
.* TLS +GLOBAL DEFAULT +UND gd
.* TLS +GLOBAL DEFAULT +8 le0
.* NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +8 ld0
.* TLS +GLOBAL DEFAULT +8 le1
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +6 _start
.* NOTYPE +GLOBAL DEFAULT +ABS __end
.* TLS +GLOBAL DEFAULT +8 ld2
.* TLS +GLOBAL DEFAULT +8 ld1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
.* TLS +GLOBAL DEFAULT +8 gd0
.* TLS +GLOBAL DEFAULT +8 ie0
