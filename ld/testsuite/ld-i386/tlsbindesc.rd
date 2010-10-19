#source: tlsbindesc.s
#source: tlsbin.s
#as: --32
#ld: -melf_i386 tmpdir/libtlslib.so
#readelf: -Ssrl
#target: i?86-*-*

There are 15 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] \.interp +.*
  \[ 2\] \.hash +.*
  \[ 3\] \.dynsym +.*
  \[ 4\] \.dynstr +.*
  \[ 5\] \.rel.dyn +.*
  \[ 6\] \.text +PROGBITS +0+8049000 .*
  \[ 7\] \.tdata +PROGBITS +0+804a000 [0-9a-f]+ 000060 00 WAT  0   0 4096
  \[ 8\] \.tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ 000040 00 WAT  0   0  1
  \[ 9\] \.dynamic +DYNAMIC +0+804a060 .*
  \[10\] \.got +PROGBITS +0+804a0e0 .*
  \[11\] \.got\.plt +PROGBITS +0+804a104 .*
  \[12\] \.shstrtab +.*
  \[13\] \.symtab +.*
  \[14\] \.strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is EXEC \(Executable file\)
Entry point 0x8049158
There are 6 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  PHDR.*
  INTERP.*
.*Requesting program interpreter.*
  LOAD.*
  LOAD.*
  DYNAMIC.*
  TLS +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x0+60 0x0+a0 R +0x1000

 Section to Segment mapping:
  Segment Sections...
   00 +
   01 +.interp *
   02 +.interp .hash .dynsym .dynstr .rel.dyn .text *
   03 +.tdata .dynamic .got .got.plt *
   04 +.dynamic *
   05 +.tdata .tbss *

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 9 entries:
 Offset +Info +Type +Sym.Value +Sym. Name
0+804a0e0  0000010e R_386_TLS_TPOFF +0+ +sG3
0+804a0e4  0000020e R_386_TLS_TPOFF +0+ +sG5
0+804a0e8  0000030e R_386_TLS_TPOFF +0+ +sG7
0+804a0ec  00000425 R_386_TLS_TPOFF32 0+ +sG2
0+804a0f0  00000525 R_386_TLS_TPOFF32 0+ +sG4
0+804a0f4  0000050e R_386_TLS_TPOFF +0+ +sG4
0+804a0f8  00000725 R_386_TLS_TPOFF32 0+ +sG6
0+804a0fc  00000825 R_386_TLS_TPOFF32 0+ +sG1
0+804a100  00000b0e R_386_TLS_TPOFF +0+ +sG8

Symbol table '.dynsym' contains 12 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +[0-9]+: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG7
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG8

Symbol table '.symtab' contains 71 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +[0-9]+: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +1 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +2 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +3 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +4 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +5 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +6 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +8 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +9 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +10 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +11 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +12 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +13 *
 +[0-9]+: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +14 *
 +[0-9]+: 00000020 +0 TLS +LOCAL  DEFAULT +7 sl1
 +[0-9]+: 00000024 +0 TLS +LOCAL  DEFAULT +7 sl2
 +[0-9]+: 00000028 +0 TLS +LOCAL  DEFAULT +7 sl3
 +[0-9]+: 0000002c +0 TLS +LOCAL  DEFAULT +7 sl4
 +[0-9]+: 00000030 +0 TLS +LOCAL  DEFAULT +7 sl5
 +[0-9]+: 00000034 +0 TLS +LOCAL  DEFAULT +7 sl6
 +[0-9]+: 00000038 +0 TLS +LOCAL  DEFAULT +7 sl7
 +[0-9]+: 0000003c +0 TLS +LOCAL  DEFAULT +7 sl8
 +[0-9]+: 00000080 +0 TLS +LOCAL  DEFAULT +8 bl1
 +[0-9]+: 00000084 +0 TLS +LOCAL  DEFAULT +8 bl2
 +[0-9]+: 00000088 +0 TLS +LOCAL  DEFAULT +8 bl3
 +[0-9]+: 0000008c +0 TLS +LOCAL  DEFAULT +8 bl4
 +[0-9]+: 00000090 +0 TLS +LOCAL  DEFAULT +8 bl5
 +[0-9]+: 00000094 +0 TLS +LOCAL  DEFAULT +8 bl6
 +[0-9]+: 00000098 +0 TLS +LOCAL  DEFAULT +8 bl7
 +[0-9]+: 0000009c +0 TLS +LOCAL  DEFAULT +8 bl8
 +[0-9]+: 00000000 +0 TLS +LOCAL  HIDDEN +7 _TLS_MODULE_BASE_
 +[0-9]+: 0+804a060 +0 OBJECT  LOCAL  HIDDEN    9 _DYNAMIC
 +[0-9]+: [0-9a-f]+ +0 OBJECT  LOCAL  HIDDEN   11 _GLOBAL_OFFSET_TABLE_
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +[0-9]+: 0000001c +0 TLS +GLOBAL DEFAULT +7 sg8
 +[0-9]+: 0000007c +0 TLS +GLOBAL DEFAULT +8 bg8
 +[0-9]+: 00000074 +0 TLS +GLOBAL DEFAULT +8 bg6
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +[0-9]+: 00000068 +0 TLS +GLOBAL DEFAULT +8 bg3
 +[0-9]+: 00000008 +0 TLS +GLOBAL DEFAULT +7 sg3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG7
 +[0-9]+: 00000048 +0 TLS +GLOBAL HIDDEN +7 sh3
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +[0-9]+: 0000000c +0 TLS +GLOBAL DEFAULT +7 sg4
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +[0-9]+: 00000010 +0 TLS +GLOBAL DEFAULT +7 sg5
 +[0-9]+: 00000070 +0 TLS +GLOBAL DEFAULT +8 bg5
 +[0-9]+: 00000058 +0 TLS +GLOBAL HIDDEN +7 sh7
 +[0-9]+: 0000005c +0 TLS +GLOBAL HIDDEN +7 sh8
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT +7 sg1
 +[0-9]+: 0+8049158 +0 FUNC +GLOBAL DEFAULT +6 _start
 +[0-9]+: 0000004c +0 TLS +GLOBAL HIDDEN +7 sh4
 +[0-9]+: 00000078 +0 TLS +GLOBAL DEFAULT +8 bg7
 +[0-9]+: 00000050 +0 TLS +GLOBAL HIDDEN +7 sh5
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +[0-9]+: 0+8049000 +0 FUNC +GLOBAL DEFAULT +6 fn2
 +[0-9]+: 00000004 +0 TLS +GLOBAL DEFAULT +7 sg2
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +[0-9]+: 00000040 +0 TLS +GLOBAL HIDDEN +7 sh1
 +[0-9]+: 00000014 +0 TLS +GLOBAL DEFAULT +7 sg6
 +[0-9]+: 00000018 +0 TLS +GLOBAL DEFAULT +7 sg7
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +[0-9]+: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +[0-9]+: 00000044 +0 TLS +GLOBAL HIDDEN +7 sh2
 +[0-9]+: 00000054 +0 TLS +GLOBAL HIDDEN +7 sh6
 +[0-9]+: 0+ +0 TLS +GLOBAL DEFAULT  UND sG8
 +[0-9]+: 00000064 +0 TLS +GLOBAL DEFAULT +8 bg2
 +[0-9]+: 00000060 +0 TLS +GLOBAL DEFAULT +8 bg1
 +[0-9]+: 0000006c +0 TLS +GLOBAL DEFAULT +8 bg4
