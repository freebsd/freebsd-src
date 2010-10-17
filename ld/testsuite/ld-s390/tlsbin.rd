#source: tlsbinpic.s
#source: tlsbin.s
#as: -m31
#ld: -shared -melf_s390
#readelf: -Ssrl
#target: s390-*-*

There are 19 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
  \[ 1\] .interp +.*
  \[ 2\] .hash +.*
  \[ 3\] .dynsym +.*
  \[ 4\] .dynstr +.*
  \[ 5\] .rela.dyn +.*
  \[ 6\] .rela.plt +.*
  \[ 7\] .plt +.*
  \[ 8\] .text +PROGBITS +0+4002e4 0+2e4 0+198 00 +AX +0 +0 +4
  \[ 9\] .data +.*
  \[10\] .tdata +PROGBITS +0+40147c 0+47c 0+60 00 WAT +0 +0 +1
  \[11\] .tbss +NOBITS +0+4014dc 0+4dc 0+40 00 WAT +0 +0 +1
  \[12\] .dynamic +DYNAMIC +0+4014dc 0+4dc 0+a0 08 +WA +4 +0 +4
  \[13\] .got +PROGBITS +0+40157c 0+57c 0+2c 04 +WA +0 +0 +4
  \[14\] .sbss +.*
  \[15\] .bss +.*
  \[16\] .shstrtab +.*
  \[17\] .symtab +.*
  \[18\] .strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is EXEC \(Executable file\)
Entry point 0x400414
There are 6 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  PHDR +0x0+34 0x0+400034 0x0+400034 0x0+c0 0x0+c0 R E 0x4
  INTERP +0x0+f4 0x0+4000f4 0x0+4000f4 0x0+11 0x0+11 R +0x1
.*Requesting program interpreter.*
  LOAD +0x0+ 0x0+400000 0x0+400000 0x0+47c 0x0+47c R E 0x1000
  LOAD +0x0+47c 0x0+40147c 0x0+40147c 0x0+12c 0x0+12c RW  0x1000
  DYNAMIC +0x0+4dc 0x0+4014dc 0x0+4014dc 0x0+a0 0x0+a0 RW  0x4
  TLS +0x0+47c 0x0+40147c 0x0+40147c 0x0+60 0x0+a0 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 *
   01 +.interp *
   02 +.interp .hash .dynsym .dynstr .rela.dyn .rela.plt .plt .text *
   03 +.tdata .tbss .dynamic .got *
   04 +.tbss .dynamic *
   05 +.tdata .tbss *

Relocation section '.rela.dyn' at offset 0x268 contains 4 entries:
 Offset +Info +Type +Sym.Value  Sym. Name \+ Addend
[0-9a-f]+  0+138 R_390_TLS_TPOFF +0+ +sG3 \+ 0
[0-9a-f]+  0+338 R_390_TLS_TPOFF +0+ +sG2 \+ 0
[0-9a-f]+  0+638 R_390_TLS_TPOFF +0+ +sG6 \+ 0
[0-9a-f]+  0+738 R_390_TLS_TPOFF +0+ +sG1 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entries:
 Offset +Info +Type +Sym.Value  Sym. Name \+ Addend
[0-9a-f]+  0+40b R_390_JMP_SLOT +0+40+2c4 +__tls_get_offset \+ 0

Symbol table '.dynsym' contains 11 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND 
 +1: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +2: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +3: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +4: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT  UND __tls_get_offset
 +5: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +6: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +7: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +8: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +9: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +10: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 71 entries:
 +Num: +Value  Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE  LOCAL  DEFAULT  UND 
 +1: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +1 
 +2: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +2 
 +3: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +3 
 +4: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +4 
 +5: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +5 
 +6: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +6 
 +7: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +7 
 +8: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +8 
 +9: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +9 
 +10: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +10 
 +11: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +11 
 +12: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +12 
 +13: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +13 
 +14: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +14 
 +15: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +15 
 +16: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +16 
 +17: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +17 
 +18: [0-9a-f]+ +0 SECTION LOCAL  DEFAULT +18 
 +19: 0+20 +0 TLS +LOCAL  DEFAULT +10 sl1
 +20: 0+24 +0 TLS +LOCAL  DEFAULT +10 sl2
 +21: 0+28 +0 TLS +LOCAL  DEFAULT +10 sl3
 +22: 0+2c +0 TLS +LOCAL  DEFAULT +10 sl4
 +23: 0+30 +0 TLS +LOCAL  DEFAULT +10 sl5
 +24: 0+34 +0 TLS +LOCAL  DEFAULT +10 sl6
 +25: 0+38 +0 TLS +LOCAL  DEFAULT +10 sl7
 +26: 0+3c +0 TLS +LOCAL  DEFAULT +10 sl8
 +27: 0+80 +0 TLS +LOCAL  DEFAULT +11 bl1
 +28: 0+84 +0 TLS +LOCAL  DEFAULT +11 bl2
 +29: 0+88 +0 TLS +LOCAL  DEFAULT +11 bl3
 +30: 0+8c +0 TLS +LOCAL  DEFAULT +11 bl4
 +31: 0+90 +0 TLS +LOCAL  DEFAULT +11 bl5
 +32: 0+94 +0 TLS +LOCAL  DEFAULT +11 bl6
 +33: 0+98 +0 TLS +LOCAL  DEFAULT +11 bl7
 +34: 0+9c +0 TLS +LOCAL  DEFAULT +11 bl8
 +35: 0+ +0 TLS +GLOBAL DEFAULT  UND sG3
 +36: 0+1c +0 TLS +GLOBAL DEFAULT +10 sg8
 +37: 0+7c +0 TLS +GLOBAL DEFAULT +11 bg8
 +38: 0+74 +0 TLS +GLOBAL DEFAULT +11 bg6
 +39: 0+68 +0 TLS +GLOBAL DEFAULT +11 bg3
 +40: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +41: 0+8 +0 TLS +GLOBAL DEFAULT +10 sg3
 +42: 0+48 +0 TLS +GLOBAL HIDDEN +10 sh3
 +43: 0+ +0 TLS +GLOBAL DEFAULT  UND sG2
 +44: 0+c +0 TLS +GLOBAL DEFAULT +10 sg4
 +45: 0+10 +0 TLS +GLOBAL DEFAULT +10 sg5
 +46: 0+70 +0 TLS +GLOBAL DEFAULT +11 bg5
 +47: 0+58 +0 TLS +GLOBAL HIDDEN +10 sh7
 +48: 0+5c +0 TLS +GLOBAL HIDDEN +10 sh8
 +49: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT  UND __tls_get_offset
 +50: 0+ +0 TLS +GLOBAL DEFAULT +10 sg1
 +51: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +8 _start
 +52: 0+4c +0 TLS +GLOBAL HIDDEN +10 sh4
 +53: 0+78 +0 TLS +GLOBAL DEFAULT +11 bg7
 +54: 0+50 +0 TLS +GLOBAL HIDDEN +10 sh5
 +55: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +56: 0+ +0 TLS +GLOBAL DEFAULT  UND sG6
 +57: [0-9a-f]+ +0 FUNC +GLOBAL DEFAULT +8 fn2
 +58: 0+4 +0 TLS +GLOBAL DEFAULT +10 sg2
 +59: 0+ +0 TLS +GLOBAL DEFAULT  UND sG1
 +60: 0+40 +0 TLS +GLOBAL HIDDEN +10 sh1
 +61: 0+14 +0 TLS +GLOBAL DEFAULT +10 sg6
 +62: 0+18 +0 TLS +GLOBAL DEFAULT +10 sg7
 +63: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +64: [0-9a-f]+ +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +65: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
 +66: 0+44 +0 TLS +GLOBAL HIDDEN +10 sh2
 +67: 0+54 +0 TLS +GLOBAL HIDDEN +10 sh6
 +68: 0+64 +0 TLS +GLOBAL DEFAULT +11 bg2
 +69: 0+60 +0 TLS +GLOBAL DEFAULT +11 bg1
 +70: 0+6c +0 TLS +GLOBAL DEFAULT +11 bg4
