#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS ELF reloc 5
#as: -32

.*: +file format elf.*mips.*

Disassembly of section \.text:
0+000000 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*0: R_MIPS_HI16	dg1
0+000004 <[^>]*> [26]4a50000 	(|d)addiu	a1,a1,0
[ 	]*4: R_MIPS_LO16	dg1
0+000008 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*8: R_MIPS_HI16	dg1
0+00000c <[^>]*> [26]4a5000c 	(|d)addiu	a1,a1,12
[ 	]*c: R_MIPS_LO16	dg1
0+000010 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*10: R_MIPS_HI16	dg1
0+000014 <[^>]*> [26]4a50000 	(|d)addiu	a1,a1,0
[ 	]*14: R_MIPS_LO16	dg1
0+000018 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+00001c <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*1c: R_MIPS_HI16	dg1
0+000020 <[^>]*> [26]4a5000c 	(|d)addiu	a1,a1,12
[ 	]*20: R_MIPS_LO16	dg1
0+000024 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+000028 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*28: R_MIPS_HI16	dg1
0+00002c <[^>]*> 8ca50000 	lw	a1,0\(a1\)
[ 	]*2c: R_MIPS_LO16	dg1
0+000030 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*30: R_MIPS_HI16	dg1
0+000034 <[^>]*> 8ca5000c 	lw	a1,12\(a1\)
[ 	]*34: R_MIPS_LO16	dg1
0+000038 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*38: R_MIPS_HI16	dg1
0+00003c <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+000040 <[^>]*> 8ca50000 	lw	a1,0\(a1\)
[ 	]*40: R_MIPS_LO16	dg1
0+000044 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*44: R_MIPS_HI16	dg1
0+000048 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+00004c <[^>]*> 8ca5000c 	lw	a1,12\(a1\)
[ 	]*4c: R_MIPS_LO16	dg1
0+000050 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*50: R_MIPS_HI16	\.data
0+000054 <[^>]*> [26]4a5003c 	(|d)addiu	a1,a1,60
[ 	]*54: R_MIPS_LO16	\.data
0+000058 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*58: R_MIPS_HI16	\.data
0+00005c <[^>]*> [26]4a50048 	(|d)addiu	a1,a1,72
[ 	]*5c: R_MIPS_LO16	\.data
0+000060 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*60: R_MIPS_HI16	\.data
0+000064 <[^>]*> [26]4a5003c 	(|d)addiu	a1,a1,60
[ 	]*64: R_MIPS_LO16	\.data
0+000068 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+00006c <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*6c: R_MIPS_HI16	\.data
0+000070 <[^>]*> [26]4a50048 	(|d)addiu	a1,a1,72
[ 	]*70: R_MIPS_LO16	\.data
0+000074 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+000078 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*78: R_MIPS_HI16	\.data
0+00007c <[^>]*> 8ca5003c 	lw	a1,60\(a1\)
[ 	]*7c: R_MIPS_LO16	\.data
0+000080 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*80: R_MIPS_HI16	\.data
0+000084 <[^>]*> 8ca50048 	lw	a1,72\(a1\)
[ 	]*84: R_MIPS_LO16	\.data
0+000088 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*88: R_MIPS_HI16	\.data
0+00008c <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+000090 <[^>]*> 8ca5003c 	lw	a1,60\(a1\)
[ 	]*90: R_MIPS_LO16	\.data
0+000094 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*94: R_MIPS_HI16	\.data
0+000098 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+00009c <[^>]*> 8ca50048 	lw	a1,72\(a1\)
[ 	]*9c: R_MIPS_LO16	\.data
0+0000a0 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*a0: R_MIPS_HI16	dg2
0+0000a4 <[^>]*> [26]4a50000 	(|d)addiu	a1,a1,0
[ 	]*a4: R_MIPS_LO16	dg2
0+0000a8 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*a8: R_MIPS_HI16	dg2
0+0000ac <[^>]*> [26]4a5000c 	(|d)addiu	a1,a1,12
[ 	]*ac: R_MIPS_LO16	dg2
0+0000b0 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*b0: R_MIPS_HI16	dg2
0+0000b4 <[^>]*> [26]4a50000 	(|d)addiu	a1,a1,0
[ 	]*b4: R_MIPS_LO16	dg2
0+0000b8 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+0000bc <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*bc: R_MIPS_HI16	dg2
0+0000c0 <[^>]*> [26]4a5000c 	(|d)addiu	a1,a1,12
[ 	]*c0: R_MIPS_LO16	dg2
0+0000c4 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+0000c8 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*c8: R_MIPS_HI16	dg2
0+0000cc <[^>]*> 8ca50000 	lw	a1,0\(a1\)
[ 	]*cc: R_MIPS_LO16	dg2
0+0000d0 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*d0: R_MIPS_HI16	dg2
0+0000d4 <[^>]*> 8ca5000c 	lw	a1,12\(a1\)
[ 	]*d4: R_MIPS_LO16	dg2
0+0000d8 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*d8: R_MIPS_HI16	dg2
0+0000dc <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+0000e0 <[^>]*> 8ca50000 	lw	a1,0\(a1\)
[ 	]*e0: R_MIPS_LO16	dg2
0+0000e4 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*e4: R_MIPS_HI16	dg2
0+0000e8 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+0000ec <[^>]*> 8ca5000c 	lw	a1,12\(a1\)
[ 	]*ec: R_MIPS_LO16	dg2
0+0000f0 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*f0: R_MIPS_HI16	\.data
0+0000f4 <[^>]*> [26]4a500b4 	(|d)addiu	a1,a1,180
[ 	]*f4: R_MIPS_LO16	\.data
0+0000f8 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*f8: R_MIPS_HI16	\.data
0+0000fc <[^>]*> [26]4a500c0 	(|d)addiu	a1,a1,192
[ 	]*fc: R_MIPS_LO16	\.data
0+000100 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*100: R_MIPS_HI16	\.data
0+000104 <[^>]*> [26]4a500b4 	(|d)addiu	a1,a1,180
[ 	]*104: R_MIPS_LO16	\.data
0+000108 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+00010c <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*10c: R_MIPS_HI16	\.data
0+000110 <[^>]*> [26]4a500c0 	(|d)addiu	a1,a1,192
[ 	]*110: R_MIPS_LO16	\.data
0+000114 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+000118 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*118: R_MIPS_HI16	\.data
0+00011c <[^>]*> 8ca500b4 	lw	a1,180\(a1\)
[ 	]*11c: R_MIPS_LO16	\.data
0+000120 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*120: R_MIPS_HI16	\.data
0+000124 <[^>]*> 8ca500c0 	lw	a1,192\(a1\)
[ 	]*124: R_MIPS_LO16	\.data
0+000128 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*128: R_MIPS_HI16	\.data
0+00012c <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+000130 <[^>]*> 8ca500b4 	lw	a1,180\(a1\)
[ 	]*130: R_MIPS_LO16	\.data
0+000134 <[^>]*> 3c050000 	lui	a1,0x0
[ 	]*134: R_MIPS_HI16	\.data
0+000138 <[^>]*> 00b1282[1d] 	(|d)addu	a1,a1,s1
0+00013c <[^>]*> 8ca500c0 	lw	a1,192\(a1\)
[ 	]*13c: R_MIPS_LO16	\.data
	\.\.\.
