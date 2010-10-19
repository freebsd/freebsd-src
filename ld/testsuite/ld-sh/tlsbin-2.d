#source: tlsbinpic.s
#source: tlsbin.s
#as: -little
#ld: -EL tmpdir/tlsbin-0-dso.so
#readelf: -Ssrl
#target: sh*-*-linux* sh*-*-netbsd*

There are [0-9]+ section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] \.interp .*
  \[ 2\] \.hash .*
  \[ 3\] \.dynsym .*
  \[ 4\] \.dynstr .*
  \[ 5\] \.rela\.dyn .*
  \[ 6\] \.rela\.plt .*
  \[ 7\] \.plt .*
  \[ 8\] \.text +PROGBITS .*
  \[ 9\] \.tdata +PROGBITS .* 0+018 00 WAT  0   0  4
  \[10\] \.tbss +NOBITS .* 0+010 00 WAT  0   0  1
#...
  \[[0-9a-f]+\] \.got +PROGBITS .*
#...
  \[[0-9a-f]+\] \.shstrtab .*
  \[[0-9a-f]+\] \.symtab .*
  \[[0-9a-f]+\] \.strtab .*
Key to Flags:
.*
.*
.*

Elf file type is EXEC \(Executable file\)
Entry point 0x402000
There are [0-9]+ program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  PHDR.*
  INTERP.*
.*Requesting program interpreter.*
  LOAD.*
  LOAD.*
  DYNAMIC.*
  TLS +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x0+18 0x0+28 R +0x4

 Section to Segment mapping:
  Segment Sections\.\.\.
   00 +
   01 +\.interp *
   02 +\.interp \.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.plt \.text *
   03 +\.tdata \.dynamic \.got *
   04 +\.dynamic *
   05 +\.tdata \.tbss *

Relocation section '\.rela\.dyn' at offset 0x[0-9a-f]+ contains 4 entries:
 Offset +Info +Type +Sym\.Value +Sym\. Name \+ Addend
[0-9a-f ]+R_SH_TLS_TPOFF32 +0+ +sG3 \+ 0
[0-9a-f ]+R_SH_TLS_TPOFF32 +0+ +sG2 \+ 0
[0-9a-f ]+R_SH_TLS_TPOFF32 +0+ +sG4 \+ 0
[0-9a-f ]+R_SH_TLS_TPOFF32 +0+ +sG1 \+ 0

Relocation section '\.rela\.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 Offset +Info +Type +Sym\.Value +Sym\. Name \+ Addend
[0-9a-f ]+R_SH_JMP_SLOT[0-9a-f ]+__tls_get_addr \+ [0-9a-f]+

Symbol table '\.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
.* NOTYPE +LOCAL +DEFAULT  UND *
.* TLS +GLOBAL DEFAULT  UND sG3
.* TLS +GLOBAL DEFAULT  UND sG2
.* TLS +GLOBAL DEFAULT  UND sG4
.* FUNC +GLOBAL DEFAULT  UND __tls_get_addr
#...
.* TLS +GLOBAL DEFAULT  UND sG1
#...

#pass
