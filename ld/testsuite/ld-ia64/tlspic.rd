#source: tlspic1.s
#source: tlspic2.s
#as:
#ld: -shared
#readelf: -WSsrl
#target: ia64-*-*

There are 21 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
  \[ 1\] .hash +.*
  \[ 2\] .dynsym +.*
  \[ 3\] .dynstr +.*
  \[ 4\] .rela.dyn +.*
  \[ 5\] .rela.IA_64.pltof +.*
  \[ 6\] .plt +.*
  \[ 7\] .text +PROGBITS +0+1000 0+1000 0+110 00 +AX +0 +0 4096
  \[ 8\] .IA_64.unwind_inf +.*
  \[ 9\] .IA_64.unwind +.*
  \[10\] .data +.*
  \[11\] .tdata +PROGBITS +0+12000 0+2000 0+60 00 WAT +0 +0 +4
  \[12\] .tbss +NOBITS +0+12060 0+2060 0+20 00 WAT +0 +0 +1
  \[13\] .dynamic +DYNAMIC +0+12060 0+2060 0+140 10 +WA +3 +0 +8
  \[14\] .got +PROGBITS +0+121a0 0+21a0 0+50 00 WAp +0 +0 +8
  \[15\] .IA_64.pltoff +.*
  \[16\] .sbss +.*
  \[17\] .bss +.*
  \[18\] .shstrtab +.*
  \[19\] .symtab +.*
  \[20\] .strtab +.*
Key to Flags:
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x1000
There are 5 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  LOAD +0x0+ 0x0+ 0x0+ 0x0+1140 0x0+1140 R E 0x10000
  LOAD +0x0+2000 0x0+12000 0x0+12000 0x0+200 0x0+200 RW +0x10000
  DYNAMIC +0x0+2060 0x0+12060 0x0+12060 0x0+140 0x0+140 RW +0x8
  TLS +0x0+2000 0x0+12000 0x0+12000 0x0+60 0x0+80 R +0x4
  IA_64_UNWIND +0x0+1128 0x0+1128 0x0+1128 0x0+18 0x0+18 R +0x8
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 6 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
0+121b8 +0+18000000a7 R_IA64_DTPMOD64LSB +0+ sg1 \+ 0
0+121c0 +0+18000000b7 R_IA64_DTPREL64LSB +0+ sg1 \+ 0
0+121c8 +0+1b00000097 R_IA64_TPREL64LSB +0+4 sg2 \+ 0
0+121d0 +0+a7 R_IA64_DTPMOD64LSB +0+
0+121d8 +0+97 R_IA64_TPREL64LSB +0+44
0+121e8 +0+97 R_IA64_TPREL64LSB +0+24

Relocation section '.rela.IA_64.pltoff' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
0+[0-9a-f]+ +0+1700000081 R_IA64_IPLTLSB +0+ __tls_get_addr \+ 0

Symbol table '.dynsym' contains 33 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND *
 +1: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +1 *
 +2: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +2 *
 +3: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +3 *
 +4: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +4 *
 +5: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +5 *
 +6: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +6 *
 +7: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +7 *
 +8: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +8 *
 +9: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +9 *
 +10: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +10 *
 +11: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +11 *
 +12: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +12 *
 +13: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +13 *
 +14: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +14 *
 +15: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +15 *
 +16: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +16 *
 +17: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +17 *
 +18: 0+1c +0 TLS +GLOBAL DEFAULT +11 sg8
 +19: 0+12060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +20: 0+8 +0 TLS +GLOBAL DEFAULT +11 sg3
 +21: 0+c +0 TLS +GLOBAL DEFAULT +11 sg4
 +22: 0+10 +0 TLS +GLOBAL DEFAULT +11 sg5
 +23: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
 +24: 0+ +0 TLS +GLOBAL DEFAULT +11 sg1
 +25: 0+1000 +272 FUNC +GLOBAL DEFAULT +7 fn1
 +26: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +27: 0+4 +0 TLS +GLOBAL DEFAULT +11 sg2
 +28: 0+14 +0 TLS +GLOBAL DEFAULT +11 sg6
 +29: 0+18 +0 TLS +GLOBAL DEFAULT +11 sg7
 +30: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +31: 0+121a0 +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +32: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 60 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND *
 +1: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +1 *
 +2: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +2 *
 +3: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +3 *
 +4: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +4 *
 +5: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +5 *
 +6: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +6 *
 +7: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +7 *
 +8: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +8 *
 +9: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +9 *
 +10: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +10 *
 +11: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +11 *
 +12: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +12 *
 +13: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +13 *
 +14: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +14 *
 +15: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +15 *
 +16: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +16 *
 +17: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +17 *
 +18: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +18 *
 +19: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +19 *
 +20: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +20 *
 +21: 0+20 +0 TLS +LOCAL +DEFAULT +11 sl1
 +22: 0+24 +0 TLS +LOCAL +DEFAULT +11 sl2
 +23: 0+28 +0 TLS +LOCAL +DEFAULT +11 sl3
 +24: 0+2c +0 TLS +LOCAL +DEFAULT +11 sl4
 +25: 0+30 +0 TLS +LOCAL +DEFAULT +11 sl5
 +26: 0+34 +0 TLS +LOCAL +DEFAULT +11 sl6
 +27: 0+38 +0 TLS +LOCAL +DEFAULT +11 sl7
 +28: 0+3c +0 TLS +LOCAL +DEFAULT +11 sl8
 +29: 0+60 +0 TLS +LOCAL +HIDDEN +12 sH1
 +30: 0+48 +0 TLS +LOCAL +HIDDEN +11 sh3
 +31: 0+64 +0 TLS +LOCAL +HIDDEN +12 sH2
 +32: 0+78 +0 TLS +LOCAL +HIDDEN +12 sH7
 +33: 0+58 +0 TLS +LOCAL +HIDDEN +11 sh7
 +34: 0+5c +0 TLS +LOCAL +HIDDEN +11 sh8
 +35: 0+6c +0 TLS +LOCAL +HIDDEN +12 sH4
 +36: 0+4c +0 TLS +LOCAL +HIDDEN +11 sh4
 +37: 0+68 +0 TLS +LOCAL +HIDDEN +12 sH3
 +38: 0+50 +0 TLS +LOCAL +HIDDEN +11 sh5
 +39: 0+70 +0 TLS +LOCAL +HIDDEN +12 sH5
 +40: 0+74 +0 TLS +LOCAL +HIDDEN +12 sH6
 +41: 0+7c +0 TLS +LOCAL +HIDDEN +12 sH8
 +42: 0+40 +0 TLS +LOCAL +HIDDEN +11 sh1
 +43: 0+44 +0 TLS +LOCAL +HIDDEN +11 sh2
 +44: 0+54 +0 TLS +LOCAL +HIDDEN +11 sh6
 +45: 0+1c +0 TLS +GLOBAL DEFAULT +11 sg8
 +46: 0+12060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +47: 0+8 +0 TLS +GLOBAL DEFAULT +11 sg3
 +48: 0+c +0 TLS +GLOBAL DEFAULT +11 sg4
 +49: 0+10 +0 TLS +GLOBAL DEFAULT +11 sg5
 +50: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
 +51: 0+ +0 TLS +GLOBAL DEFAULT +11 sg1
 +52: 0+1000 +272 FUNC +GLOBAL DEFAULT +7 fn1
 +53: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +54: 0+4 +0 TLS +GLOBAL DEFAULT +11 sg2
 +55: 0+14 +0 TLS +GLOBAL DEFAULT +11 sg6
 +56: 0+18 +0 TLS +GLOBAL DEFAULT +11 sg7
 +57: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +58: 0+121a0 +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +59: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
