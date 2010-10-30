#source: tls32.s
#source: tlslib32.s
#as: -a32
#ld: -melf32ppc
#readelf: -WSsrl
#target: powerpc*-*-*

There are 16 section headers.*

Section Headers:
 +\[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
 +\[ 1\] \.interp +.*
 +\[ 2\] \.hash +.*
 +\[ 3\] \.dynsym +.*
 +\[ 4\] \.dynstr +.*
 +\[ 5\] \.rela\.dyn +.*
 +\[ 6\] \.rela\.plt +.*
 +\[ 7\] \.text +PROGBITS +[0-9a-f]+ [0-9a-f]+ 000070 00 +AX +0 +0 +1
 +\[ 8\] \.tdata +PROGBITS +[0-9a-f]+ [0-9a-f]+ 00001c 00 WAT +0 +0 +4
 +\[ 9\] \.tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ 00001c 00 WAT +0 +0 +4
 +\[10\] \.dynamic +DYNAMIC +[0-9a-f]+ [0-9a-f]+ [0-9a-f]+ 08 +WA +4 +0 +4
 +\[11\] \.got +PROGBITS +[0-9a-f]+ [0-9a-f]+ 00001c 04 WAX +0 +0 +4
 +\[12\] \.plt +NOBITS +.*
 +\[13\] \.shstrtab +STRTAB +.*
 +\[14\] \.symtab +SYMTAB +.*
 +\[15\] \.strtab +STRTAB +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point .*
There are 6 program headers, starting at offset 52

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz MemSiz +Flg Align
 +PHDR +0x000034 0x01800034 0x01800034 0x000c0 0x000c0 R E 0x4
 +INTERP +0x0000f4 0x018000f4 0x018000f4 0x00011 0x00011 R +0x1
 +\[Requesting program interpreter: .*\]
 +LOAD .* R E 0x10000
 +LOAD .* RWE 0x10000
 +DYNAMIC .* RW +0x4
 +TLS .* 0x0001c 0x00038 R +0x4

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +00 +
 +01 +\.interp 
 +02 +\.interp \.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.text 
 +03 +\.tdata \.dynamic \.got \.plt 
 +04 +\.dynamic 
 +05 +\.tdata \.tbss 

Relocation section '\.rela\.dyn' at offset .* contains 2 entries:
 Offset +Info +Type +Sym\. Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC_TPREL32 +00000000 +gd \+ 0
[0-9a-f ]+R_PPC_DTPMOD32 +00000000 +ld \+ 0

Relocation section '\.rela\.plt' at offset .* contains 1 entries:
 Offset +Info +Type +Sym\. Value +Symbol's Name \+ Addend
[0-9a-f ]+R_PPC_JMP_SLOT[0-9a-f ]+__tls_get_addr \+ 0

Symbol table '\.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT +UND 
.* TLS +GLOBAL DEFAULT +UND gd
.* FUNC +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +ABS __end
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '\.symtab' contains 37 entries:
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
.* TLS +LOCAL +DEFAULT +8 gd4
.* TLS +LOCAL +DEFAULT +8 ld4
.* TLS +LOCAL +DEFAULT +8 ld5
.* TLS +LOCAL +DEFAULT +8 ld6
.* TLS +LOCAL +DEFAULT +8 ie4
.* TLS +LOCAL +DEFAULT +8 le4
.* TLS +LOCAL +DEFAULT +8 le5
.* OBJECT +LOCAL +HIDDEN +10 _DYNAMIC
.* OBJECT +LOCAL +HIDDEN +11 _GLOBAL_OFFSET_TABLE_
.* TLS +GLOBAL DEFAULT +UND gd
.* TLS +GLOBAL DEFAULT +9 le0
.* FUNC +GLOBAL DEFAULT +UND __tls_get_addr
.* TLS +GLOBAL DEFAULT +9 ld0
.* TLS +GLOBAL DEFAULT +9 le1
.* TLS +GLOBAL DEFAULT +UND ld
.* NOTYPE +GLOBAL DEFAULT +7 _start
.* NOTYPE +GLOBAL DEFAULT +ABS __end
.* TLS +GLOBAL DEFAULT +9 ld2
.* TLS +GLOBAL DEFAULT +9 ld1
.* NOTYPE +GLOBAL DEFAULT +ABS __bss_start
.* NOTYPE +GLOBAL DEFAULT +ABS _edata
.* NOTYPE +GLOBAL DEFAULT +ABS _end
.* TLS +GLOBAL DEFAULT +9 gd0
.* TLS +GLOBAL DEFAULT +9 ie0
