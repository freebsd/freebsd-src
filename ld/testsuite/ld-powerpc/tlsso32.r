#source: tls32.s
#as: -a32
#ld: -shared -melf32ppc
#readelf: -WSsrl
#target: powerpc*-*-*

There are 20 section headers.*

Section Headers:
 +\[Nr\] Name +Type +Addr +Off +Size +ES Flg Lk Inf Al
 +\[ 0\] +NULL +0+ 0+ 0+ 0+ +0 +0 +0
 +\[ 1\] \.hash +HASH +0+b4 0+b4 0+dc 04 +A +2 +0 +4
 +\[ 2\] \.dynsym +DYNSYM +0+190 0+190 0+240 10 +A +3 +11 +4
 +\[ 3\] \.dynstr +STRTAB +0+3d0 0+3d0 0+81 0+ +A +0 +0 +1
 +\[ 4\] \.rela\.dyn +RELA +0+454 0+454 0+d8 0c +A +2 +0 +4
 +\[ 5\] \.rela\.plt +RELA +0+52c 0+52c 0+c 0c +A +2 +f +4
 +\[ 6\] \.text +PROGBITS +0+538 0+538 0+70 0+ +AX +0 +0 +1
 +\[ 7\] \.data +PROGBITS +0+105a8 0+5a8 0+ 0+ +WA +0 +0 +1
 +\[ 8\] \.tdata +PROGBITS +0+105a8 0+5a8 0+1c 0+ WAT +0 +0 +4
 +\[ 9\] \.tbss +NOBITS +0+105c4 0+5c4 0+1c 0+ WAT +0 +0 +4
 +\[10\] \.dynamic +DYNAMIC +0+105c4 0+5c4 0+a0 08 +WA +3 +0 +4
 +\[11\] \.got +PROGBITS +0+10664 0+664 0+34 04 WAX +0 +0 +4
 +\[12\] \.sdata2 +PROGBITS +0+10698 0+698 0+ 0+ +A +0 +0 +4
 +\[13\] \.sdata +PROGBITS +0+10698 0+698 0+ 0+ +WA +0 +0 +4
 +\[14\] \.sbss +NOBITS +0+10698 0+698 0+ 0+ +WA +0 +0 +1
 +\[15\] \.plt +NOBITS +0+10698 0+698 0+54 0+ WAX +0 +0 +4
 +\[16\] \.bss +NOBITS +0+106ec 0+698 0+ 0+ +WA +0 +0 +1
 +\[17\] \.shstrtab +STRTAB +0+ 0+698 0+86 0+ +0 +0 +1
 +\[18\] \.symtab +SYMTAB +0+ 0+a40 0+2e0 10 +19 +1b +4
 +\[19\] \.strtab +STRTAB +0+ 0+d20 0+a9 0+ +0 +0 +1
#...

Elf file type is DYN \(Shared object file\)
Entry point 0x538
There are 4 program headers.*

Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz MemSiz +Flg Align
 +LOAD +0x0+ 0x0+ 0x0+ 0x0+5a8 0x0+5a8 R E 0x10000
 +LOAD +0x0+5a8 0x0+105a8 0x0+105a8 0x0+f0 0x0+144 RWE 0x10000
 +DYNAMIC +0x0+5c4 0x0+105c4 0x0+105c4 0x0+a0 0x0+a0 RW +0x4
 +TLS +0x0+5a8 0x0+105a8 0x0+105a8 0x0+1c 0x0+38 R +0x4

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +0+ +\.hash \.dynsym \.dynstr \.rela\.dyn \.rela\.plt \.text 
 +01 +\.tdata \.tbss \.dynamic \.got \.plt 
 +02 +\.tbss \.dynamic 
 +03 +\.tdata \.tbss 

Relocation section '\.rela\.dyn' at offset 0x454 contains 18 entries:
 Offset +Info +Type +Sym\. Value +Symbol's Name \+ Addend
0+53c +0+140a R_PPC_REL24 +0+ +__tls_get_addr \+ 0
0+544 +0+140a R_PPC_REL24 +0+ +__tls_get_addr \+ 0
0+57c +0+140a R_PPC_REL24 +0+ +__tls_get_addr \+ 0
0+584 +0+140a R_PPC_REL24 +0+ +__tls_get_addr \+ 0
0+56e +0+1345 R_PPC_TPREL16 +0+30 +le0 \+ 0
0+572 +0+1648 R_PPC_TPREL16_HA +0+34 +le1 \+ 0
0+576 +0+1646 R_PPC_TPREL16_LO +0+34 +le1 \+ 0
0+59e +0+845 R_PPC_TPREL16 +0+105a8 +\.tdata \+ 105bc
0+5a2 +0+848 R_PPC_TPREL16_HA +0+105a8 +\.tdata \+ 105c0
0+5a6 +0+846 R_PPC_TPREL16_LO +0+105a8 +\.tdata \+ 105c0
0+10674 +0+44 R_PPC_DTPMOD32 +0+
0+1067c +0+44 R_PPC_DTPMOD32 +0+
0+10680 +0+4e R_PPC_DTPREL32 +0+
0+10684 +0+1244 R_PPC_DTPMOD32 +0+ +gd \+ 0
0+10688 +0+124e R_PPC_DTPREL32 +0+ +gd \+ 0
0+1068c +0+2144 R_PPC_DTPMOD32 +0+1c +gd0 \+ 0
0+10690 +0+214e R_PPC_DTPREL32 +0+1c +gd0 \+ 0
0+10694 +0+2249 R_PPC_TPREL32 +0+2c +ie0 \+ 0

Relocation section '\.rela\.plt' at offset 0x52c contains 1 entries:
 Offset +Info +Type +Sym\. Value +Symbol's Name \+ Addend
0+106e0 +0+1415 R_PPC_JMP_SLOT +0+ +__tls_get_addr \+ 0

Symbol table '\.dynsym' contains 36 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: 0+b4 +0 SECTION LOCAL +DEFAULT +1 
 +2: 0+190 +0 SECTION LOCAL +DEFAULT +2 
 +3: 0+3d0 +0 SECTION LOCAL +DEFAULT +3 
 +4: 0+454 +0 SECTION LOCAL +DEFAULT +4 
 +5: 0+52c +0 SECTION LOCAL +DEFAULT +5 
 +6: 0+538 +0 SECTION LOCAL +DEFAULT +6 
 +7: 0+105a8 +0 SECTION LOCAL +DEFAULT +7 
 +8: 0+105a8 +0 SECTION LOCAL +DEFAULT +8 
 +9: 0+105c4 +0 SECTION LOCAL +DEFAULT +9 
 +10: 0+105c4 +0 SECTION LOCAL +DEFAULT +10 
 +11: 0+10664 +0 SECTION LOCAL +DEFAULT +11 
 +12: 0+10698 +0 SECTION LOCAL +DEFAULT +12 
 +13: 0+10698 +0 SECTION LOCAL +DEFAULT +13 
 +14: 0+10698 +0 SECTION LOCAL +DEFAULT +14 
 +15: 0+10698 +0 SECTION LOCAL +DEFAULT +15 
 +16: 0+106ec +0 SECTION LOCAL +DEFAULT +16 
 +17: 0+105c4 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +18: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND gd
 +19: 0+30 +0 TLS +GLOBAL DEFAULT +9 le0
 +20: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
 +21: 0+20 +0 TLS +GLOBAL DEFAULT +9 ld0
 +22: 0+34 +0 TLS +GLOBAL DEFAULT +9 le1
 +23: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND ld
 +24: 0+538 +0 NOTYPE +GLOBAL DEFAULT +6 _start
 +25: 0+106ec +0 NOTYPE +GLOBAL DEFAULT +ABS __end
 +26: 0+18698 +0 OBJECT +GLOBAL DEFAULT +13 _SDA_BASE_
 +27: 0+28 +0 TLS +GLOBAL DEFAULT +9 ld2
 +28: 0+24 +0 TLS +GLOBAL DEFAULT +9 ld1
 +29: 0+10698 +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +30: 0+10698 +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +31: 0+10668 +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +32: 0+106ec +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +33: 0+1c +0 TLS +GLOBAL DEFAULT +9 gd0
 +34: 0+2c +0 TLS +GLOBAL DEFAULT +9 ie0
 +35: 0+18698 +0 OBJECT +GLOBAL DEFAULT +12 _SDA2_BASE_

Symbol table '\.symtab' contains 46 entries:
 +Num: +Value +Size Type +Bind +Vis +Ndx Name
 +0: 0+ +0 NOTYPE +LOCAL +DEFAULT +UND 
 +1: 0+b4 +0 SECTION LOCAL +DEFAULT +1 
 +2: 0+190 +0 SECTION LOCAL +DEFAULT +2 
 +3: 0+3d0 +0 SECTION LOCAL +DEFAULT +3 
 +4: 0+454 +0 SECTION LOCAL +DEFAULT +4 
 +5: 0+52c +0 SECTION LOCAL +DEFAULT +5 
 +6: 0+538 +0 SECTION LOCAL +DEFAULT +6 
 +7: 0+105a8 +0 SECTION LOCAL +DEFAULT +7 
 +8: 0+105a8 +0 SECTION LOCAL +DEFAULT +8 
 +9: 0+105c4 +0 SECTION LOCAL +DEFAULT +9 
 +10: 0+105c4 +0 SECTION LOCAL +DEFAULT +10 
 +11: 0+10664 +0 SECTION LOCAL +DEFAULT +11 
 +12: 0+10698 +0 SECTION LOCAL +DEFAULT +12 
 +13: 0+10698 +0 SECTION LOCAL +DEFAULT +13 
 +14: 0+10698 +0 SECTION LOCAL +DEFAULT +14 
 +15: 0+10698 +0 SECTION LOCAL +DEFAULT +15 
 +16: 0+106ec +0 SECTION LOCAL +DEFAULT +16 
 +17: 0+ +0 SECTION LOCAL +DEFAULT +17 
 +18: 0+ +0 SECTION LOCAL +DEFAULT +18 
 +19: 0+ +0 SECTION LOCAL +DEFAULT +19 
 +20: 0+ +0 TLS +LOCAL +DEFAULT +8 gd4
 +21: 0+4 +0 TLS +LOCAL +DEFAULT +8 ld4
 +22: 0+8 +0 TLS +LOCAL +DEFAULT +8 ld5
 +23: 0+c +0 TLS +LOCAL +DEFAULT +8 ld6
 +24: 0+10 +0 TLS +LOCAL +DEFAULT +8 ie4
 +25: 0+14 +0 TLS +LOCAL +DEFAULT +8 le4
 +26: 0+18 +0 TLS +LOCAL +DEFAULT +8 le5
 +27: 0+105c4 +0 OBJECT +GLOBAL DEFAULT +ABS _DYNAMIC
 +28: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND gd
 +29: 0+30 +0 TLS +GLOBAL DEFAULT +9 le0
 +30: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND __tls_get_addr
 +31: 0+20 +0 TLS +GLOBAL DEFAULT +9 ld0
 +32: 0+34 +0 TLS +GLOBAL DEFAULT +9 le1
 +33: 0+ +0 NOTYPE +GLOBAL DEFAULT +UND ld
 +34: 0+538 +0 NOTYPE +GLOBAL DEFAULT +6 _start
 +35: 0+106ec +0 NOTYPE +GLOBAL DEFAULT +ABS __end
 +36: 0+18698 +0 OBJECT +GLOBAL DEFAULT +13 _SDA_BASE_
 +37: 0+28 +0 TLS +GLOBAL DEFAULT +9 ld2
 +38: 0+24 +0 TLS +GLOBAL DEFAULT +9 ld1
 +39: 0+10698 +0 NOTYPE +GLOBAL DEFAULT +ABS __bss_start
 +40: 0+10698 +0 NOTYPE +GLOBAL DEFAULT +ABS _edata
 +41: 0+10668 +0 OBJECT +GLOBAL DEFAULT +ABS _GLOBAL_OFFSET_TABLE_
 +42: 0+106ec +0 NOTYPE +GLOBAL DEFAULT +ABS _end
 +43: 0+1c +0 TLS +GLOBAL DEFAULT +9 gd0
 +44: 0+2c +0 TLS +GLOBAL DEFAULT +9 ie0
 +45: 0+18698 +0 OBJECT +GLOBAL DEFAULT +12 _SDA2_BASE_
