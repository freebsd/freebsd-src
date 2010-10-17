#source: tlsbinpic.s
#source: tlsbin.s
#as:
#ld: -shared
#readelf: -WSsrl
#target: ia64-*-*

There are 22 section headers, starting at offset 0x[0-9a-f]+:

Section Headers:
  \[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
  \[ 0\] +NULL +0+ 0+ 0+ 00 +0 +0 +0
  \[ 1\] .interp +.*
  \[ 2\] .hash +.*
  \[ 3\] .dynsym +.*
  \[ 4\] .dynstr +.*
  \[ 5\] .rela.dyn +.*
  \[ 6\] .rela.IA_64.pltof +.*
  \[ 7\] .plt +.*
  \[ 8\] .text +PROGBITS +40+1000 0+1000 0+140 00 +AX +0 +0 4096
  \[ 9\] .IA_64.unwind_inf +.*
  \[10\] .IA_64.unwind +.*
  \[11\] .data +.*
  \[12\] .tdata +PROGBITS +60+2000 0+2000 0+60 00 WAT +0 +0 +4
  \[13\] .tbss +NOBITS +60+2060 0+2060 0+40 00 WAT +0 +0 +1
  \[14\] .dynamic +DYNAMIC +60+2060 0+2060 0+150 10 +WA +4 +0 +8
  \[15\] .got +PROGBITS +60+21b0 0+21b0 0+48 00 WAp +0 +0 +8
  \[16\] .IA_64.pltoff +.*
  \[17\] .sbss +.*
  \[18\] .bss +.*
  \[19\] .shstrtab +.*
  \[20\] .symtab +.*
  \[21\] .strtab +.*
#...

Elf file type is EXEC \(Executable file\)
Entry point 0x40+10d0
There are 7 program headers, starting at offset [0-9]+

Program Headers:
  Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
  PHDR +0x0+40 0x40+40 0x40+40 0x0+188 0x0+188 R E 0x8
  INTERP +0x0+1c8 0x40+1c8 0x40+1c8 0x[0-9a-f]+ 0x[0-9a-f]+ R +0x1
.*Requesting program interpreter.*
  LOAD +0x0+ 0x40+ 0x40+ 0x0+1170 0x0+1170 R E 0x10000
  LOAD +0x0+2000 0x60+2000 0x60+2000 0x0+210 0x0+210 RW +0x10000
  DYNAMIC +0x0+2060 0x60+2060 0x60+2060 0x0+150 0x0+150 RW +0x8
  TLS +0x0+2000 0x60+2000 0x60+2000 0x0+60 0x0+a0 R +0x4
  IA_64_UNWIND .* R +0x8
#...

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 3 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
60+21c8  0+200000097 R_IA64_TPREL64LSB +0+ sG2 \+ 0
60+21d0  0+5000000a7 R_IA64_DTPMOD64LSB +0+ sG1 \+ 0
60+21d8  0+5000000b7 R_IA64_DTPREL64LSB +0+ sG1 \+ 0

Relocation section '.rela.IA_64.pltoff' at offset 0x[0-9a-f]+ contains 1 entries:
 +Offset +Info +Type +Symbol's Value  Symbol's Name \+ Addend
60+2200  0+300000081 R_IA64_IPLTLSB +0+ __tls_get_addr \+ 0

Symbol table '.dynsym' contains 9 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND *
 +1: 60+2060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +2: 0+ +0 TLS +GLOBAL DEFAULT +UND sG2
 +3: 0+ +16 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
 +4: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +5: 0+ +0 TLS +GLOBAL DEFAULT +UND sG1
 +6: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +7: 60+21b0 +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +8: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end

Symbol table '.symtab' contains 72 entries:
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
 +21: [0-9a-f]+ +0 SECTION LOCAL +DEFAULT +21 *
 +22: 0+20 +0 TLS +LOCAL +DEFAULT +12 sl1
 +23: 0+24 +0 TLS +LOCAL +DEFAULT +12 sl2
 +24: 0+28 +0 TLS +LOCAL +DEFAULT +12 sl3
 +25: 0+2c +0 TLS +LOCAL +DEFAULT +12 sl4
 +26: 0+30 +0 TLS +LOCAL +DEFAULT +12 sl5
 +27: 0+34 +0 TLS +LOCAL +DEFAULT +12 sl6
 +28: 0+38 +0 TLS +LOCAL +DEFAULT +12 sl7
 +29: 0+3c +0 TLS +LOCAL +DEFAULT +12 sl8
 +30: 0+80 +0 TLS +LOCAL +DEFAULT +13 bl1
 +31: 0+84 +0 TLS +LOCAL +DEFAULT +13 bl2
 +32: 0+88 +0 TLS +LOCAL +DEFAULT +13 bl3
 +33: 0+8c +0 TLS +LOCAL +DEFAULT +13 bl4
 +34: 0+90 +0 TLS +LOCAL +DEFAULT +13 bl5
 +35: 0+94 +0 TLS +LOCAL +DEFAULT +13 bl6
 +36: 0+98 +0 TLS +LOCAL +DEFAULT +13 bl7
 +37: 0+9c +0 TLS +LOCAL +DEFAULT +13 bl8
 +38: 0+1c +0 TLS +GLOBAL DEFAULT +12 sg8
 +39: 0+7c +0 TLS +GLOBAL DEFAULT +13 bg8
 +40: 0+74 +0 TLS +GLOBAL DEFAULT +13 bg6
 +41: 0+68 +0 TLS +GLOBAL DEFAULT +13 bg3
 +42: 60+2060 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +43: 0+8 +0 TLS +GLOBAL DEFAULT +12 sg3
 +44: 0+48 +0 TLS +GLOBAL HIDDEN +12 sh3
 +45: 0+ +0 TLS +GLOBAL DEFAULT +UND sG2
 +46: 0+c +0 TLS +GLOBAL DEFAULT +12 sg4
 +47: 0+10 +0 TLS +GLOBAL DEFAULT +12 sg5
 +48: 0+70 +0 TLS +GLOBAL DEFAULT +13 bg5
 +49: 0+ +16 FUNC +GLOBAL DEFAULT +UND __tls_get_addr
 +50: 0+58 +0 TLS +GLOBAL HIDDEN +12 sh7
 +51: 0+5c +0 TLS +GLOBAL HIDDEN +12 sh8
 +52: 0+ +0 TLS +GLOBAL DEFAULT +12 sg1
 +53: 40+10d0 +112 FUNC +GLOBAL DEFAULT +8 _start
 +54: 0+4c +0 TLS +GLOBAL HIDDEN +12 sh4
 +55: 0+78 +0 TLS +GLOBAL DEFAULT +13 bg7
 +56: 0+50 +0 TLS +GLOBAL HIDDEN +12 sh5
 +57: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +58: 40+1000 +208 FUNC +GLOBAL DEFAULT +8 fn2
 +59: 0+4 +0 TLS +GLOBAL DEFAULT +12 sg2
 +60: 0+ +0 TLS +GLOBAL DEFAULT +UND sG1
 +61: 0+40 +0 TLS +GLOBAL HIDDEN +12 sh1
 +62: 0+14 +0 TLS +GLOBAL DEFAULT +12 sg6
 +63: 0+18 +0 TLS +GLOBAL DEFAULT +12 sg7
 +64: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +65: [0-9a-f]+ +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +66: [0-9a-f]+ +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +67: 0+44 +0 TLS +GLOBAL HIDDEN +12 sh2
 +68: 0+54 +0 TLS +GLOBAL HIDDEN +12 sh6
 +69: 0+64 +0 TLS +GLOBAL DEFAULT +13 bg2
 +70: 0+60 +0 TLS +GLOBAL DEFAULT +13 bg1
 +71: 0+6c +0 TLS +GLOBAL DEFAULT +13 bg4
