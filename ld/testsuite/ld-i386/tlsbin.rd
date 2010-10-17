#source: tlsbinpic.s
#source: tlsbin.s
#as: --32
#ld: -melf_i386 tmpdir/libtlslib.so
#readelf: -Ssrl
#target: i?86-*-*

There are 18 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
  \[ 1\] \.interp +.*
  \[ 2\] \.hash +.*
  \[ 3\] \.dynsym +.*
  \[ 4\] \.dynstr +.*
  \[ 5\] \.rel.dyn +.*
  \[ 6\] \.rel.plt +.*
  \[ 7\] \.plt +.*
  \[ 8\] \.text +PROGBITS +0+8049000 .*
  \[ 9\] \.data +.*
  \[10\] .tdata +PROGBITS +0+804a000 [0-9a-f]+ 000060 00 WAT  0   0  1
  \[11\] .tbss +NOBITS +[0-9a-f]+ [0-9a-f]+ 000040 00 WAT  0   0  1
  \[12\] \.dynamic +DYNAMIC +0+804a060 .*
  \[13\] \.got +PROGBITS +0+804a100 .*
  \[14\] \.bss +.*
  \[15\] \.shstrtab +.*
  \[16\] \.symtab +.*
  \[17\] \.strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is EXEC \(Executable file\)
Entry point 0x8049178
There are 6 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  PHDR.*
  INTERP.*
.*Requesting program interpreter.*
  LOAD.*
  LOAD.*
  DYNAMIC.*
  TLS +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x0+60 0x0+a0 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 +
   01 +.interp *
   02 +.interp .hash .dynsym .dynstr .rel.dyn .rel.plt .plt .text *
   03 +.tdata .tbss .dynamic .got *
   04 +.tbss .dynamic *
   05 +.tdata .tbss *

Relocation section '.rel.dyn' at offset 0x[0-9a-f]+ contains 9 entries:
 Offset +Info +Type +Sym.Value +Sym. Name
0+804a110  0000010e R_386_TLS_TPOFF +0+ +sG3
0+804a114  0000020e R_386_TLS_TPOFF +0+ +sG5
0+804a118  0000040e R_386_TLS_TPOFF +0+ +sG7
0+804a11c  00000525 R_386_TLS_TPOFF32 0+ +sG2
0+804a120  00000625 R_386_TLS_TPOFF32 0+ +sG4
0+804a124  0000060e R_386_TLS_TPOFF +0+ +sG4
0+804a128  00000825 R_386_TLS_TPOFF32 0+ +sG6
0+804a12c  00000925 R_386_TLS_TPOFF32 0+ +sG1
0+804a130  00000d0e R_386_TLS_TPOFF +0+ +sG8

Relocation section '.rel.plt' at offset 0x30c contains 1 entries:
 Offset +Info +Type +Sym.Value  Sym. Name
0+804a10c  00000e07 R_386_JUMP_SLOT +[0-9a-f]+ +___tls_get_addr

Symbol table '.dynsym' contains 15 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND *
 +1: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +2: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +3: 0+804a060 +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +4: 0+ +0 TLS +GLOBAL DEFAULT  UND sG7
 +5: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +6: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +7: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +8: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +9: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +10: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +11: 0+804a100 +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +12: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +13: 0+ +0 TLS +GLOBAL DEFAULT  UND sG8
 +14: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT  UND ___tls_get_addr

Symbol table '.symtab' contains 74 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
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
 +18: 00000020 +0 TLS +LOCAL  DEFAULT +10 sl1
 +19: 00000024 +0 TLS +LOCAL  DEFAULT +10 sl2
 +20: 00000028 +0 TLS +LOCAL  DEFAULT +10 sl3
 +21: 0000002c +0 TLS +LOCAL  DEFAULT +10 sl4
 +22: 00000030 +0 TLS +LOCAL  DEFAULT +10 sl5
 +23: 00000034 +0 TLS +LOCAL  DEFAULT +10 sl6
 +24: 00000038 +0 TLS +LOCAL  DEFAULT +10 sl7
 +25: 0000003c +0 TLS +LOCAL  DEFAULT +10 sl8
 +26: 00000080 +0 TLS +LOCAL  DEFAULT +11 bl1
 +27: 00000084 +0 TLS +LOCAL  DEFAULT +11 bl2
 +28: 00000088 +0 TLS +LOCAL  DEFAULT +11 bl3
 +29: 0000008c +0 TLS +LOCAL  DEFAULT +11 bl4
 +30: 00000090 +0 TLS +LOCAL  DEFAULT +11 bl5
 +31: 00000094 +0 TLS +LOCAL  DEFAULT +11 bl6
 +32: 00000098 +0 TLS +LOCAL  DEFAULT +11 bl7
 +33: 0000009c +0 TLS +LOCAL  DEFAULT +11 bl8
 +34: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +35: 0000001c +0 TLS +GLOBAL DEFAULT +10 sg8
 +36: 0000007c +0 TLS +GLOBAL DEFAULT +11 bg8
 +37: 00000074 +0 TLS +GLOBAL DEFAULT +11 bg6
 +38: 0+ +0 TLS +GLOBAL DEFAULT  UND sG5
 +39: 00000068 +0 TLS +GLOBAL DEFAULT +11 bg3
 +40: 0+804a060 +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +41: 00000008 +0 TLS +GLOBAL DEFAULT +10 sg3
 +42: 0+ +0 TLS +GLOBAL DEFAULT  UND sG7
 +43: 00000048 +0 TLS +GLOBAL HIDDEN +10 sh3
 +44: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +45: 0000000c +0 TLS +GLOBAL DEFAULT +10 sg4
 +46: 0+ +0 TLS +GLOBAL DEFAULT  UND sG4
 +47: 00000010 +0 TLS +GLOBAL DEFAULT +10 sg5
 +48: 00000070 +0 TLS +GLOBAL DEFAULT +11 bg5
 +49: 00000058 +0 TLS +GLOBAL HIDDEN +10 sh7
 +50: 0000005c +0 TLS +GLOBAL HIDDEN +10 sh8
 +51: 0+ +0 TLS +GLOBAL DEFAULT +10 sg1
 +52: 0+8049178 +0 FUNC +GLOBAL DEFAULT +8 _start
 +53: 0000004c +0 TLS +GLOBAL HIDDEN +10 sh4
 +54: 00000078 +0 TLS +GLOBAL DEFAULT +11 bg7
 +55: 00000050 +0 TLS +GLOBAL HIDDEN +10 sh5
 +56: 0+804a134 +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +57: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +58: 0+8049000 +0 FUNC +GLOBAL DEFAULT +8 fn2
 +59: 00000004 +0 TLS +GLOBAL DEFAULT +10 sg2
 +60: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +61: 00000040 +0 TLS +GLOBAL HIDDEN +10 sh1
 +62: 00000014 +0 TLS +GLOBAL DEFAULT +10 sg6
 +63: 00000018 +0 TLS +GLOBAL DEFAULT +10 sg7
 +64: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +65: 0+804a100 +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +66: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +67: 00000044 +0 TLS +GLOBAL HIDDEN +10 sh2
 +68: 00000054 +0 TLS +GLOBAL HIDDEN +10 sh6
 +69: 0+ +0 TLS +GLOBAL DEFAULT  UND sG8
 +70: 00000064 +0 TLS +GLOBAL DEFAULT +11 bg2
 +71: 00000060 +0 TLS +GLOBAL DEFAULT +11 bg1
 +72: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT  UND ___tls_get_addr
 +73: 0000006c +0 TLS +GLOBAL DEFAULT +11 bg4
