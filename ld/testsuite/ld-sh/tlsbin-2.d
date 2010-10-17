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
  \[ 1\] \.interp +.*
  \[ 2\] \.hash +.*
  \[ 3\] \.dynsym +.*
  \[ 4\] \.dynstr +.*
  \[ 5\] \.rela\.dyn +.*
  \[ 6\] \.rela\.plt +.*
  \[ 7\] \.plt +.*
  \[ 8\] \.text +PROGBITS +0+401000 .*
  \[ 9\] \.data +.*
  \[10\] \.tdata +PROGBITS +0+413000 [0-9a-f]+ 0+018 00 WAT  0   0  4
  \[11\] \.tbss +NOBITS +0+413018 [0-9a-f]+ 0+010 00 WAT  0   0  1
  \[12\] \.dynamic +DYNAMIC +0+413018 .*
#...
  \[[0-9a-f]+\] \.got +PROGBITS +0+4130b8 .*
  \[[0-9a-f]+\] \.sbss +.*
  \[[0-9a-f]+\] \.bss +.*
#...
  \[[0-9a-f]+\] \.shstrtab +.*
  \[[0-9a-f]+\] \.symtab +.*
  \[[0-9a-f]+\] \.strtab +.*
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
   03 +\.tdata \.tbss \.dynamic \.got *
   04 +\.tbss \.dynamic *
   05 +\.tdata \.tbss *

Relocation section '\.rela\.dyn' at offset 0x[0-9a-f]+ contains 4 entries:
 Offset +Info +Type +Sym\.Value +Sym\. Name \+ Addend
0+4130c8  00000197 R_SH_TLS_TPOFF32 +0+ +sG3 \+ 0
0+4130cc  00000397 R_SH_TLS_TPOFF32 +0+ +sG2 \+ 0
0+4130d0  00000497 R_SH_TLS_TPOFF32 +0+ +sG4 \+ 0
0+4130d4  0000[0-9a-f]+97 R_SH_TLS_TPOFF32 +0+ +sG1 \+ 0

Relocation section '\.rela\.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 Offset +Info +Type +Sym\.Value +Sym\. Name \+ Addend
0+4130c4  000005a4 R_SH_JMP_SLOT +[0-9a-f]+ +__tls_get_addr \+ [0-9a-f]+

Symbol table '\.dynsym' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT  UND *
 +1: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +2: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +3: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +4: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +5: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT  UND __tls_get_addr
#...
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
#...

Symbol table '\.symtab' contains [0-9]+ entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +1: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +1 *
 +2: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +2 *
 +3: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +3 *
 +4: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +4 *
 +5: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +5 *
 +6: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +6 *
 +7: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 *
 +8: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +8 *
 +9: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +9 *
 +10: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +10 *
 +11: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +11 *
 +12: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +12 *
 +13: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +13 *
 +14: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +14 *
 +15: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +15 *
 +16: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +16 *
 +17: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +17 *
 +18: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +18 *
#...
 +[0-9]+: 00000008 +0 TLS +LOCAL  DEFAULT +10 sl1
 +[0-9]+: 0000000c +0 TLS +LOCAL  DEFAULT +10 sl2
 +[0-9]+: 00000020 +0 TLS +LOCAL  DEFAULT +11 bl1
 +[0-9]+: 00000024 +0 TLS +LOCAL  DEFAULT +11 bl2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +[0-9]+: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +[0-9]+: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT  UND __tls_get_addr
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT +10 sg1
 +[0-9]+: 0+402000 +0 FUNC +GLOBAL DEFAULT +8 _start
#...
 +[0-9]+: 0+401000 +0 FUNC +GLOBAL DEFAULT +8 fn2
#...
 +[0-9]+: 00000004 +0 TLS +GLOBAL DEFAULT +10 sg2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +[0-9]+: 00000010 +0 TLS +GLOBAL HIDDEN +10 sh1
 +[0-9]+: 004130d8 +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +[0-9]+: 00000014 +0 TLS +GLOBAL HIDDEN +10 sh2
 +[0-9]+: 0000001c +0 TLS +GLOBAL DEFAULT +11 bg2
 +[0-9]+: 00000018 +0 TLS +GLOBAL DEFAULT +11 bg1
#...
 +[0-9]+: .*
#pass
