#source: tlspic1.s
#source: tlspic2.s
#as: --64
#ld: -shared -melf_x86_64
#readelf: -WSsrl
#target: x86_64-*-*

There are 17 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
  \[ 1\] .hash +.*
  \[ 2\] .dynsym +.*
  \[ 3\] .dynstr +.*
  \[ 4\] .rela.dyn +.*
  \[ 5\] .rela.plt +.*
  \[ 6\] .plt +.*
  \[ 7\] .text +PROGBITS +0+1000 0+1000 0+1ac 00 +AX +0 +0 4096
  \[ 8\] .data +.*
  \[ 9\] .tdata +PROGBITS +0+102000 0+2000 0+60 00 WAT +0 +0 +1
  \[10\] .tbss +NOBITS +0+102060 0+2060 0+20 00 WAT +0 +0 +1
  \[11\] .dynamic +DYNAMIC +0+102060 0+2060 0+130 10 +WA +3 +0 +8
  \[12\] .got +PROGBITS +0+102190 0+2190 0+b0 08 +WA +0 +0 +8
  \[13\] .bss +.*
  \[14\] .shstrtab +.*
  \[15\] .symtab +.*
  \[16\] .strtab +.*
Key to Flags:
.*
.*
.*

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 4 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  LOAD +0x0+ 0x0+ 0x0+ 0x[0-9a-f]+ 0x[0-9a-f]+ R E 0x100000
  LOAD +0x0+2000 0x0+102000 0x0+102000 0x0+240 0x0+240 RW +0x100000
  DYNAMIC +0x0+2060 0x0+102060 0x0+102060 0x0+130 0x0+130 RW +0x8
  TLS +0x0+2000 0x0+102000 0x0+102000 0x0+60 0x0+80 R +0x1

 Section to Segment mapping:
  Segment Sections...
   00 +.hash .dynsym .dynstr .rela.dyn .rela.plt .plt .text *
   01 +.tdata .tbss .dynamic .got *
   02 +.tbss .dynamic *
   03 +.tdata .tbss *

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 14 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
0+1021b0  0+10 R_X86_64_DTPMOD64 +0+
0+1021c0  0+12 R_X86_64_TPOFF64 +0+24
0+1021c8  0+12 R_X86_64_TPOFF64 +0+30
0+1021d0  0+10 R_X86_64_DTPMOD64 +0+
0+1021e0  0+10 R_X86_64_DTPMOD64 +0+
0+1021f0  0+12 R_X86_64_TPOFF64 +0+64
0+102210  0+12 R_X86_64_TPOFF64 +0+50
0+102218  0+12 R_X86_64_TPOFF64 +0+70
0+102228  0+10 R_X86_64_DTPMOD64 +0+
0+102238  0+12 R_X86_64_TPOFF64 +0+44
0+1021f8  0+1200000012 R_X86_64_TPOFF64 +0+10 sg5 \+ 0
0+102200  0+1400000010 R_X86_64_DTPMOD64 +0+ sg1 \+ 0
0+102208  0+1400000011 R_X86_64_DTPOFF64 +0+ sg1 \+ 0
0+102220  0+1700000012 R_X86_64_TPOFF64 +0+4 sg2 \+ 0

Relocation section '.rela.plt' at offset 0x658 contains 1 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
0+[0-9a-f]+  0+1300000007 R_X86_64_JUMP_SLOT +0+ __tls_get_addr \+ 0

Symbol table '.dynsym' contains 29 entries:
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
 +14: 0+1c +0 TLS +GLOBAL DEFAULT +9 sg8
 +15: 0+102060 +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +16: 0+8 +0 TLS +GLOBAL DEFAULT +9 sg3
 +17: 0+c +0 TLS +GLOBAL DEFAULT +9 sg4
 +18: 0+10 +0 TLS +GLOBAL DEFAULT +9 sg5
 +19: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
 +20: 0+ +0 TLS +GLOBAL DEFAULT +9 sg1
 +21: 0+1000 +0 FUNC +GLOBAL DEFAULT +7 fn1
 +22: 0+102240 +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +23: 0+4 +0 TLS +GLOBAL DEFAULT +9 sg2
 +24: 0+14 +0 TLS +GLOBAL DEFAULT +9 sg6
 +25: 0+18 +0 TLS +GLOBAL DEFAULT +9 sg7
 +26: 0+102240 +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +27: 0+102190 +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +28: 0+102240 +0 NOTYPE  GLOBAL DEFAULT  ABS _end

Symbol table '.symtab' contains 56 entries:
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
 +17: 0+20 +0 TLS +LOCAL  DEFAULT +9 sl1
 +18: 0+24 +0 TLS +LOCAL  DEFAULT +9 sl2
 +19: 0+28 +0 TLS +LOCAL  DEFAULT +9 sl3
 +20: 0+2c +0 TLS +LOCAL  DEFAULT +9 sl4
 +21: 0+30 +0 TLS +LOCAL  DEFAULT +9 sl5
 +22: 0+34 +0 TLS +LOCAL  DEFAULT +9 sl6
 +23: 0+38 +0 TLS +LOCAL  DEFAULT +9 sl7
 +24: 0+3c +0 TLS +LOCAL  DEFAULT +9 sl8
 +25: 0+60 +0 TLS +LOCAL  HIDDEN +10 sH1
 +26: 0+48 +0 TLS +LOCAL  HIDDEN +9 sh3
 +27: 0+64 +0 TLS +LOCAL  HIDDEN +10 sH2
 +28: 0+78 +0 TLS +LOCAL  HIDDEN +10 sH7
 +29: 0+58 +0 TLS +LOCAL  HIDDEN +9 sh7
 +30: 0+5c +0 TLS +LOCAL  HIDDEN +9 sh8
 +31: 0+6c +0 TLS +LOCAL  HIDDEN +10 sH4
 +32: 0+4c +0 TLS +LOCAL  HIDDEN +9 sh4
 +33: 0+68 +0 TLS +LOCAL  HIDDEN +10 sH3
 +34: 0+50 +0 TLS +LOCAL  HIDDEN +9 sh5
 +35: 0+70 +0 TLS +LOCAL  HIDDEN +10 sH5
 +36: 0+74 +0 TLS +LOCAL  HIDDEN +10 sH6
 +37: 0+7c +0 TLS +LOCAL  HIDDEN +10 sH8
 +38: 0+40 +0 TLS +LOCAL  HIDDEN +9 sh1
 +39: 0+44 +0 TLS +LOCAL  HIDDEN +9 sh2
 +40: 0+54 +0 TLS +LOCAL  HIDDEN +9 sh6
 +41: 0+1c +0 TLS +GLOBAL DEFAULT +9 sg8
 +42: 0+102060 +0 OBJECT  GLOBAL DEFAULT  ABS _DYNAMIC
 +43: 0+8 +0 TLS +GLOBAL DEFAULT +9 sg3
 +44: 0+c +0 TLS +GLOBAL DEFAULT +9 sg4
 +45: 0+10 +0 TLS +GLOBAL DEFAULT +9 sg5
 +46: 0+ +0 NOTYPE  GLOBAL DEFAULT  UND __tls_get_addr
 +47: 0+ +0 TLS +GLOBAL DEFAULT +9 sg1
 +48: 0+1000 +0 FUNC +GLOBAL DEFAULT +7 fn1
 +49: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS __bss_start
 +50: 0+4 +0 TLS +GLOBAL DEFAULT +9 sg2
 +51: 0+14 +0 TLS +GLOBAL DEFAULT +9 sg6
 +52: 0+18 +0 TLS +GLOBAL DEFAULT +9 sg7
 +53: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _edata
 +54: 0+102190 +0 OBJECT  GLOBAL DEFAULT  ABS _GLOBAL_OFFSET_TABLE_
 +55: [0-9a-f]+ +0 NOTYPE  GLOBAL DEFAULT  ABS _end
