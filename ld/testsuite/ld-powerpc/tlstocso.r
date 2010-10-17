#source: tlstoc.s
#as: -a64
#ld: -shared -melf64ppc
#readelf: -WSsrl
#target: powerpc64*-*-*

There are 19 section headers.*

Section Headers:
 +\[Nr\] Name +Type +Address +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] \.hash +HASH +0+120 0+120 0+c8 04 +A +2 +0 +8
 +\[ 2\] \.dynsym +DYNSYM +0+1e8 0+1e8 0+2e8 18 +A +3 +10 +8
 +\[ 3\] \.dynstr +STRTAB +0+4d0 0+4d0 0+53 0+ +A +0 +0 +1
 +\[ 4\] \.rela\.dyn +RELA +0+528 0+528 0+108 18 +A +2 +0 +8
 +\[ 5\] \.rela\.plt +RELA +0+630 0+630 0+18 18 +A +2 +e +8
 +\[ 6\] \.text +PROGBITS +0+648 0+648 0+bc 0+ +AX +0 +0 +4
 +\[ 7\] \.data +PROGBITS +0+10708 0+708 0+ 0+ +WA +0 +0 +1
 +\[ 8\] \.branch_lt +PROGBITS +0+10708 0+708 0+ 0+ +WA +0 +0 +8
 +\[ 9\] \.tdata +PROGBITS +0+10708 0+708 0+38 0+ WAT +0 +0 +8
 +\[10\] \.tbss +NOBITS +0+10740 0+740 0+38 0+ WAT +0 +0 +8
 +\[11\] \.dynamic +DYNAMIC +0+10740 0+740 0+150 10 +WA +3 +0 +8
 +\[12\] \.got +PROGBITS +0+10890 0+890 0+58 08 +WA +0 +0 +8
 +\[13\] \.sbss +NOBITS +0+108e8 0+8e8 0+ 0+ +W +0 +0 +1
 +\[14\] \.plt +NOBITS +0+108e8 0+8e8 0+30 18 +WA +0 +0 +8
 +\[15\] \.bss +NOBITS +0+10918 0+8e8 0+ 0+ +WA +0 +0 +1
 +\[16\] \.shstrtab +STRTAB +0+ 0+8e8 0+82 0+ +0 +0 +1
 +\[17\] \.symtab +SYMTAB +0+ 0+e30 0+408 18 +18 +1c +8
 +\[18\] \.strtab +STRTAB +0+ 0+1238 0+8c 0+ +0 +0 +1
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x664
There are 4 program headers.*

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x0+704 0x0+704 R E 0x10000
 +LOAD +0x0+708 0x0+10708 0x0+10708 0x0+1e0 0x0+210 RW +0x10000
 +DYNAMIC +0x0+740 0x0+10740 0x0+10740 0x0+150 0x0+150 RW +0x8
 +TLS +0x0+708 0x0+10708 0x0+10708 0x0+38 0x0+70 R +0x8

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +0+ +\.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.text 
 +01 +\.tdata \.tbss \.dynamic \.got \.plt 
 +02 +\.tbss \.dynamic 
 +03 +\.tdata \.tbss 

Relocation section '\.rela\.dyn' at offset .* contains 11 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
0+6b2 +0+1200000045 R_PPC64_TPREL16 +0+60 le0 \+ 0
0+6b6 +0+1500000048 R_PPC64_TPREL16_HA +0+68 le1 \+ 0
0+6ba +0+1500000046 R_PPC64_TPREL16_LO +0+68 le1 \+ 0
0+10898 +0+1100000044 R_PPC64_DTPMOD64 +0+ gd \+ 0
0+108a0 +0+110000004e R_PPC64_DTPREL64 +0+ gd \+ 0
0+108a8 +0+1600000044 R_PPC64_DTPMOD64 +0+ ld \+ 0
0+108b8 +0+1d00000044 R_PPC64_DTPMOD64 +0+38 gd0 \+ 0
0+108c0 +0+1d0000004e R_PPC64_DTPREL64 +0+38 gd0 \+ 0
0+108c8 +0+1400000044 R_PPC64_DTPMOD64 +0+40 ld0 \+ 0
0+108d8 +0+180000004e R_PPC64_DTPREL64 +0+50 ld2 \+ 0
0+108e0 +0+1e00000049 R_PPC64_TPREL64 +0+58 ie0 \+ 0

Relocation section '\.rela\.plt' at offset .* contains 1 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
0+10900 +0+1300000015 R_PPC64_JMP_SLOT +0+ __tls_get_addr \+ 0

Symbol table '\.dynsym' contains 31 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: 0+120 +0 SECTION LOCAL +DEFAULT +1 
 +2: 0+1e8 +0 SECTION LOCAL +DEFAULT +2 
 +3: 0+4d0 +0 SECTION LOCAL +DEFAULT +3 
 +4: 0+528 +0 SECTION LOCAL +DEFAULT +4 
 +5: 0+630 +0 SECTION LOCAL +DEFAULT +5 
 +6: 0+648 +0 SECTION LOCAL +DEFAULT +6 
 +7: 0+10708 +0 SECTION LOCAL +DEFAULT +7 
 +8: 0+10708 +0 SECTION LOCAL +DEFAULT +8 
 +9: 0+10708 +0 SECTION LOCAL +DEFAULT +9 
 +10: 0+10740 +0 SECTION LOCAL +DEFAULT +10 
 +11: 0+10740 +0 SECTION LOCAL +DEFAULT +11 
 +12: 0+10890 +0 SECTION LOCAL +DEFAULT +12 
 +13: 0+108e8 +0 SECTION LOCAL +DEFAULT +13 
 +14: 0+108e8 +0 SECTION LOCAL +DEFAULT +14 
 +15: 0+10918 +0 SECTION LOCAL +DEFAULT +15 
 +16: 0+10740 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +17: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND gd
 +18: 0+60 +0 TLS +GLOBAL DEFAULT +10 le0
 +19: 0+ +24 OBJECT +GLOBAL DEFAULT +UND __tls_get_addr
 +20: 0+40 +0 TLS +GLOBAL DEFAULT +10 ld0
 +21: 0+68 +0 TLS +GLOBAL DEFAULT +10 le1
 +22: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND ld
 +23: 0+664 +0 NOTYPE +GLOBAL DEFAULT +6 _start
 +24: 0+50 +0 TLS +GLOBAL DEFAULT +10 ld2
 +25: 0+48 +0 TLS +GLOBAL DEFAULT +10 ld1
 +26: 0+108e8 +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +27: 0+108e8 +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +28: 0+10918 +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +29: 0+38 +0 TLS +GLOBAL DEFAULT +10 gd0
 +30: 0+58 +0 TLS +GLOBAL DEFAULT +10 ie0

Symbol table '\.symtab' contains 43 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: 0+120 +0 SECTION LOCAL +DEFAULT +1 
 +2: 0+1e8 +0 SECTION LOCAL +DEFAULT +2 
 +3: 0+4d0 +0 SECTION LOCAL +DEFAULT +3 
 +4: 0+528 +0 SECTION LOCAL +DEFAULT +4 
 +5: 0+630 +0 SECTION LOCAL +DEFAULT +5 
 +6: 0+648 +0 SECTION LOCAL +DEFAULT +6 
 +7: 0+10708 +0 SECTION LOCAL +DEFAULT +7 
 +8: 0+10708 +0 SECTION LOCAL +DEFAULT +8 
 +9: 0+10708 +0 SECTION LOCAL +DEFAULT +9 
 +10: 0+10740 +0 SECTION LOCAL +DEFAULT +10 
 +11: 0+10740 +0 SECTION LOCAL +DEFAULT +11 
 +12: 0+10890 +0 SECTION LOCAL +DEFAULT +12 
 +13: 0+108e8 +0 SECTION LOCAL +DEFAULT +13 
 +14: 0+108e8 +0 SECTION LOCAL +DEFAULT +14 
 +15: 0+10918 +0 SECTION LOCAL +DEFAULT +15 
 +16: 0+ +0 SECTION LOCAL +DEFAULT +16 
 +17: 0+ +0 SECTION LOCAL +DEFAULT +17 
 +18: 0+ +0 SECTION LOCAL +DEFAULT +18 
 +19: 0+ +0 TLS +LOCAL +DEFAULT +9 gd4
 +20: 0+8 +0 TLS +LOCAL +DEFAULT +9 ld4
 +21: 0+10 +0 TLS +LOCAL +DEFAULT +9 ld5
 +22: 0+18 +0 TLS +LOCAL +DEFAULT +9 ld6
 +23: 0+20 +0 TLS +LOCAL +DEFAULT +9 ie4
 +24: 0+28 +0 TLS +LOCAL +DEFAULT +9 le4
 +25: 0+30 +0 TLS +LOCAL +DEFAULT +9 le5
 +26: 0+108e0 +0 NOTYPE +LOCAL +DEFAULT +12 \.Lie0
 +27: 0+648 +0 NOTYPE +LOCAL +DEFAULT +6 \.__tls_get_addr
 +28: 0+10740 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +29: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND gd
 +30: 0+60 +0 TLS +GLOBAL DEFAULT +10 le0
 +31: 0+ +24 OBJECT +GLOBAL DEFAULT +UND __tls_get_addr
 +32: 0+40 +0 TLS +GLOBAL DEFAULT +10 ld0
 +33: 0+68 +0 TLS +GLOBAL DEFAULT +10 le1
 +34: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND ld
 +35: 0+664 +0 NOTYPE +GLOBAL DEFAULT +6 _start
 +36: 0+50 +0 TLS +GLOBAL DEFAULT +10 ld2
 +37: 0+48 +0 TLS +GLOBAL DEFAULT +10 ld1
 +38: 0+108e8 +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +39: 0+108e8 +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +40: 0+10918 +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +41: 0+38 +0 TLS +GLOBAL DEFAULT +10 gd0
 +42: 0+58 +0 TLS +GLOBAL DEFAULT +10 ie0
