#source: tls.s
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
 +\[ 4\] \.rela\.dyn +RELA +0+528 0+528 0+180 18 +A +2 +0 +8
 +\[ 5\] \.rela\.plt +RELA +0+6a8 0+6a8 0+18 18 +A +2 +e +8
 +\[ 6\] \.text +PROGBITS +0+6c0 0+6c0 0+fc 0+ +AX +0 +0 +4
 +\[ 7\] \.data +PROGBITS +0+107c0 0+7c0 0+ 0+ +WA +0 +0 +1
 +\[ 8\] \.branch_lt +PROGBITS +0+107c0 0+7c0 0+ 0+ +WA +0 +0 +8
 +\[ 9\] \.tdata +PROGBITS +0+107c0 0+7c0 0+38 0+ WAT +0 +0 +8
 +\[10\] \.tbss +NOBITS +0+107f8 0+7f8 0+38 0+ WAT +0 +0 +8
 +\[11\] \.dynamic +DYNAMIC +0+107f8 0+7f8 0+150 10 +WA +3 +0 +8
 +\[12\] \.got +PROGBITS +0+10948 0+948 0+60 08 +WA +0 +0 +8
 +\[13\] \.sbss +NOBITS +0+109a8 0+9a8 0+ 0+ +W +0 +0 +1
 +\[14\] \.plt +NOBITS +0+109a8 0+9a8 0+30 18 +WA +0 +0 +8
 +\[15\] \.bss +NOBITS +0+109d8 0+9a8 0+ 0+ +WA +0 +0 +1
 +\[16\] \.shstrtab +STRTAB +0+ 0+9a8 0+82 0+ +0 +0 +1
 +\[17\] \.symtab +SYMTAB +0+ 0+ef0 0+3f0 18 +18 +1b +8
 +\[18\] \.strtab +STRTAB +0+ 0+12e0 0+86 0+ +0 +0 +1
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x6dc
There are 4 program headers.*

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x0+7bc 0x0+7bc R E 0x10000
 +LOAD +0x0+7c0 0x0+107c0 0x0+107c0 0x0+1e8 0x0+218 RW +0x10000
 +DYNAMIC +0x0+7f8 0x0+107f8 0x0+107f8 0x0+150 0x0+150 RW +0x8
 +TLS +0x0+7c0 0x0+107c0 0x0+107c0 0x0+38 0x0+70 R +0x8

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +0+ +\.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.text 
 +01 +\.tdata \.tbss \.dynamic \.got \.plt 
 +02 +\.tbss \.dynamic 
 +03 +\.tdata \.tbss 

Relocation section '\.rela\.dyn' at offset .* contains 16 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
0+72a +0+1200000045 R_PPC64_TPREL16 +0+60 le0 \+ 0
0+72e +0+1500000048 R_PPC64_TPREL16_HA +0+68 le1 \+ 0
0+732 +0+1500000046 R_PPC64_TPREL16_LO +0+68 le1 \+ 0
0+76a +0+90000005f R_PPC64_TPREL16_DS +0+107c0 \.tdata \+ 28
0+76e +0+900000048 R_PPC64_TPREL16_HA +0+107c0 \.tdata \+ 30
0+772 +0+900000046 R_PPC64_TPREL16_LO +0+107c0 \.tdata \+ 30
0+10950 +0+44 R_PPC64_DTPMOD64 +0+
0+10960 +0+44 R_PPC64_DTPMOD64 +0+
0+10968 +0+4e R_PPC64_DTPREL64 +0+
0+10970 +0+4e R_PPC64_DTPREL64 +0+18
0+10978 +0+1100000044 R_PPC64_DTPMOD64 +0+ gd \+ 0
0+10980 +0+110000004e R_PPC64_DTPREL64 +0+ gd \+ 0
0+10988 +0+180000004e R_PPC64_DTPREL64 +0+50 ld2 \+ 0
0+10990 +0+1d00000044 R_PPC64_DTPMOD64 +0+38 gd0 \+ 0
0+10998 +0+1d0000004e R_PPC64_DTPREL64 +0+38 gd0 \+ 0
0+109a0 +0+1e00000049 R_PPC64_TPREL64 +0+58 ie0 \+ 0

Relocation section '\.rela\.plt' at offset .* contains 1 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
0+109c0 +0+1300000015 R_PPC64_JMP_SLOT +0+ __tls_get_addr \+ 0

Symbol table '\.dynsym' contains 31 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: 0+120 +0 SECTION LOCAL +DEFAULT +1 
 +2: 0+1e8 +0 SECTION LOCAL +DEFAULT +2 
 +3: 0+4d0 +0 SECTION LOCAL +DEFAULT +3 
 +4: 0+528 +0 SECTION LOCAL +DEFAULT +4 
 +5: 0+6a8 +0 SECTION LOCAL +DEFAULT +5 
 +6: 0+6c0 +0 SECTION LOCAL +DEFAULT +6 
 +7: 0+107c0 +0 SECTION LOCAL +DEFAULT +7 
 +8: 0+107c0 +0 SECTION LOCAL +DEFAULT +8 
 +9: 0+107c0 +0 SECTION LOCAL +DEFAULT +9 
 +10: 0+107f8 +0 SECTION LOCAL +DEFAULT +10 
 +11: 0+107f8 +0 SECTION LOCAL +DEFAULT +11 
 +12: 0+10948 +0 SECTION LOCAL +DEFAULT +12 
 +13: 0+109a8 +0 SECTION LOCAL +DEFAULT +13 
 +14: 0+109a8 +0 SECTION LOCAL +DEFAULT +14 
 +15: 0+109d8 +0 SECTION LOCAL +DEFAULT +15 
 +16: 0+107f8 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +17: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND gd
 +18: 0+60 +0 TLS +GLOBAL DEFAULT +10 le0
 +19: 0+ +24 OBJECT +GLOBAL DEFAULT +UND __tls_get_addr
 +20: 0+40 +0 TLS +GLOBAL DEFAULT +10 ld0
 +21: 0+68 +0 TLS +GLOBAL DEFAULT +10 le1
 +22: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND ld
 +23: 0+6dc +0 NOTYPE +GLOBAL DEFAULT +6 _start
 +24: 0+50 +0 TLS +GLOBAL DEFAULT +10 ld2
 +25: 0+48 +0 TLS +GLOBAL DEFAULT +10 ld1
 +26: 0+109a8 +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +27: 0+109a8 +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +28: 0+109d8 +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +29: 0+38 +0 TLS +GLOBAL DEFAULT +10 gd0
 +30: 0+58 +0 TLS +GLOBAL DEFAULT +10 ie0

Symbol table '\.symtab' contains 42 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: 0+120 +0 SECTION LOCAL +DEFAULT +1 
 +2: 0+1e8 +0 SECTION LOCAL +DEFAULT +2 
 +3: 0+4d0 +0 SECTION LOCAL +DEFAULT +3 
 +4: 0+528 +0 SECTION LOCAL +DEFAULT +4 
 +5: 0+6a8 +0 SECTION LOCAL +DEFAULT +5 
 +6: 0+6c0 +0 SECTION LOCAL +DEFAULT +6 
 +7: 0+107c0 +0 SECTION LOCAL +DEFAULT +7 
 +8: 0+107c0 +0 SECTION LOCAL +DEFAULT +8 
 +9: 0+107c0 +0 SECTION LOCAL +DEFAULT +9 
 +10: 0+107f8 +0 SECTION LOCAL +DEFAULT +10 
 +11: 0+107f8 +0 SECTION LOCAL +DEFAULT +11 
 +12: 0+10948 +0 SECTION LOCAL +DEFAULT +12 
 +13: 0+109a8 +0 SECTION LOCAL +DEFAULT +13 
 +14: 0+109a8 +0 SECTION LOCAL +DEFAULT +14 
 +15: 0+109d8 +0 SECTION LOCAL +DEFAULT +15 
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
 +26: 0+6c0 +0 NOTYPE +LOCAL +DEFAULT +6 \.__tls_get_addr
 +27: 0+107f8 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +28: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND gd
 +29: 0+60 +0 TLS +GLOBAL DEFAULT +10 le0
 +30: 0+ +24 OBJECT +GLOBAL DEFAULT +UND __tls_get_addr
 +31: 0+40 +0 TLS +GLOBAL DEFAULT +10 ld0
 +32: 0+68 +0 TLS +GLOBAL DEFAULT +10 le1
 +33: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND ld
 +34: 0+6dc +0 NOTYPE +GLOBAL DEFAULT +6 _start
 +35: 0+50 +0 TLS +GLOBAL DEFAULT +10 ld2
 +36: 0+48 +0 TLS +GLOBAL DEFAULT +10 ld1
 +37: 0+109a8 +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +38: 0+109a8 +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +39: 0+109d8 +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +40: 0+38 +0 TLS +GLOBAL DEFAULT +10 gd0
 +41: 0+58 +0 TLS +GLOBAL DEFAULT +10 ie0
