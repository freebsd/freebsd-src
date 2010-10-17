#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS ELF xgot reloc n32
#as: -n32 -KPIC -xgot
#source: elf-rel-got-n32.s

.*: +file format elf32-n.*mips.*

Disassembly of section \.text:
00000000 <fn> 3c050000 	lui	a1,0x0
			0: R_MIPS_GOT_HI16	dg1
00000004 <fn\+0x4> 00bc2821 	addu	a1,a1,gp
00000008 <fn\+0x8> 8ca50000 	lw	a1,0\(a1\)
			8: R_MIPS_GOT_LO16	dg1
0000000c <fn\+0xc> 3c050000 	lui	a1,0x0
			c: R_MIPS_GOT_HI16	dg1
00000010 <fn\+0x10> 00bc2821 	addu	a1,a1,gp
00000014 <fn\+0x14> 8ca50000 	lw	a1,0\(a1\)
			14: R_MIPS_GOT_LO16	dg1
00000018 <fn\+0x18> 24a5000c 	addiu	a1,a1,12
0000001c <fn\+0x1c> 3c050000 	lui	a1,0x0
			1c: R_MIPS_GOT_HI16	dg1
00000020 <fn\+0x20> 00bc2821 	addu	a1,a1,gp
00000024 <fn\+0x24> 8ca50000 	lw	a1,0\(a1\)
			24: R_MIPS_GOT_LO16	dg1
00000028 <fn\+0x28> 3c010001 	lui	at,0x1
0000002c <fn\+0x2c> 3421e240 	ori	at,at,0xe240
00000030 <fn\+0x30> 00a12821 	addu	a1,a1,at
00000034 <fn\+0x34> 3c050000 	lui	a1,0x0
			34: R_MIPS_GOT_HI16	dg1
00000038 <fn\+0x38> 00bc2821 	addu	a1,a1,gp
0000003c <fn\+0x3c> 8ca50000 	lw	a1,0\(a1\)
			3c: R_MIPS_GOT_LO16	dg1
00000040 <fn\+0x40> 00b12821 	addu	a1,a1,s1
00000044 <fn\+0x44> 3c050000 	lui	a1,0x0
			44: R_MIPS_GOT_HI16	dg1
00000048 <fn\+0x48> 00bc2821 	addu	a1,a1,gp
0000004c <fn\+0x4c> 8ca50000 	lw	a1,0\(a1\)
			4c: R_MIPS_GOT_LO16	dg1
00000050 <fn\+0x50> 24a5000c 	addiu	a1,a1,12
00000054 <fn\+0x54> 00b12821 	addu	a1,a1,s1
00000058 <fn\+0x58> 3c050000 	lui	a1,0x0
			58: R_MIPS_GOT_HI16	dg1
0000005c <fn\+0x5c> 00bc2821 	addu	a1,a1,gp
00000060 <fn\+0x60> 8ca50000 	lw	a1,0\(a1\)
			60: R_MIPS_GOT_LO16	dg1
00000064 <fn\+0x64> 3c010001 	lui	at,0x1
00000068 <fn\+0x68> 3421e240 	ori	at,at,0xe240
0000006c <fn\+0x6c> 00a12821 	addu	a1,a1,at
00000070 <fn\+0x70> 00b12821 	addu	a1,a1,s1
00000074 <fn\+0x74> 3c050000 	lui	a1,0x0
			74: R_MIPS_GOT_HI16	dg1
00000078 <fn\+0x78> 00bc2821 	addu	a1,a1,gp
0000007c <fn\+0x7c> 8ca50000 	lw	a1,0\(a1\)
			7c: R_MIPS_GOT_LO16	dg1
00000080 <fn\+0x80> 8ca50000 	lw	a1,0\(a1\)
00000084 <fn\+0x84> 3c050000 	lui	a1,0x0
			84: R_MIPS_GOT_HI16	dg1
00000088 <fn\+0x88> 00bc2821 	addu	a1,a1,gp
0000008c <fn\+0x8c> 8ca50000 	lw	a1,0\(a1\)
			8c: R_MIPS_GOT_LO16	dg1
00000090 <fn\+0x90> 8ca5000c 	lw	a1,12\(a1\)
00000094 <fn\+0x94> 3c050000 	lui	a1,0x0
			94: R_MIPS_GOT_HI16	dg1
00000098 <fn\+0x98> 00bc2821 	addu	a1,a1,gp
0000009c <fn\+0x9c> 8ca50000 	lw	a1,0\(a1\)
			9c: R_MIPS_GOT_LO16	dg1
000000a0 <fn\+0xa0> 00b12821 	addu	a1,a1,s1
000000a4 <fn\+0xa4> 8ca50000 	lw	a1,0\(a1\)
000000a8 <fn\+0xa8> 3c050000 	lui	a1,0x0
			a8: R_MIPS_GOT_HI16	dg1
000000ac <fn\+0xac> 00bc2821 	addu	a1,a1,gp
000000b0 <fn\+0xb0> 8ca50000 	lw	a1,0\(a1\)
			b0: R_MIPS_GOT_LO16	dg1
000000b4 <fn\+0xb4> 00b12821 	addu	a1,a1,s1
000000b8 <fn\+0xb8> 8ca5000c 	lw	a1,12\(a1\)
000000bc <fn\+0xbc> 3c010000 	lui	at,0x0
			bc: R_MIPS_GOT_HI16	dg1
000000c0 <fn\+0xc0> 003c0821 	addu	at,at,gp
000000c4 <fn\+0xc4> 8c210000 	lw	at,0\(at\)
			c4: R_MIPS_GOT_LO16	dg1
000000c8 <fn\+0xc8> 00250821 	addu	at,at,a1
000000cc <fn\+0xcc> 8c250022 	lw	a1,34\(at\)
000000d0 <fn\+0xd0> 3c010000 	lui	at,0x0
			d0: R_MIPS_GOT_HI16	dg1
000000d4 <fn\+0xd4> 003c0821 	addu	at,at,gp
000000d8 <fn\+0xd8> 8c210000 	lw	at,0\(at\)
			d8: R_MIPS_GOT_LO16	dg1
000000dc <fn\+0xdc> 00250821 	addu	at,at,a1
000000e0 <fn\+0xe0> ac250038 	sw	a1,56\(at\)
000000e4 <fn\+0xe4> 3c010000 	lui	at,0x0
			e4: R_MIPS_GOT_HI16	dg1
000000e8 <fn\+0xe8> 003c0821 	addu	at,at,gp
000000ec <fn\+0xec> 8c210000 	lw	at,0\(at\)
			ec: R_MIPS_GOT_LO16	dg1
000000f0 <fn\+0xf0> 8825000[03] 	lwl	a1,[03]\(at\)
000000f4 <fn\+0xf4> 9825000[03] 	lwr	a1,[03]\(at\)
000000f8 <fn\+0xf8> 3c010000 	lui	at,0x0
			f8: R_MIPS_GOT_HI16	dg1
000000fc <fn\+0xfc> 003c0821 	addu	at,at,gp
00000100 <fn\+0x100> 8c210000 	lw	at,0\(at\)
			100: R_MIPS_GOT_LO16	dg1
00000104 <fn\+0x104> 2421000c 	addiu	at,at,12
00000108 <fn\+0x108> 8825000[03] 	lwl	a1,[03]\(at\)
0000010c <fn\+0x10c> 9825000[03] 	lwr	a1,[03]\(at\)
00000110 <fn\+0x110> 3c010000 	lui	at,0x0
			110: R_MIPS_GOT_HI16	dg1
00000114 <fn\+0x114> 003c0821 	addu	at,at,gp
00000118 <fn\+0x118> 8c210000 	lw	at,0\(at\)
			118: R_MIPS_GOT_LO16	dg1
0000011c <fn\+0x11c> 00310821 	addu	at,at,s1
00000120 <fn\+0x120> 8825000[03] 	lwl	a1,[03]\(at\)
00000124 <fn\+0x124> 9825000[03] 	lwr	a1,[03]\(at\)
00000128 <fn\+0x128> 3c010000 	lui	at,0x0
			128: R_MIPS_GOT_HI16	dg1
0000012c <fn\+0x12c> 003c0821 	addu	at,at,gp
00000130 <fn\+0x130> 8c210000 	lw	at,0\(at\)
			130: R_MIPS_GOT_LO16	dg1
00000134 <fn\+0x134> 2421000c 	addiu	at,at,12
00000138 <fn\+0x138> 00310821 	addu	at,at,s1
0000013c <fn\+0x13c> 8825000[03] 	lwl	a1,[03]\(at\)
00000140 <fn\+0x140> 9825000[03] 	lwr	a1,[03]\(at\)
00000144 <fn\+0x144> 3c010000 	lui	at,0x0
			144: R_MIPS_GOT_HI16	dg1
00000148 <fn\+0x148> 003c0821 	addu	at,at,gp
0000014c <fn\+0x14c> 8c210000 	lw	at,0\(at\)
			14c: R_MIPS_GOT_LO16	dg1
00000150 <fn\+0x150> 24210022 	addiu	at,at,34
00000154 <fn\+0x154> 00250821 	addu	at,at,a1
00000158 <fn\+0x158> 8825000[03] 	lwl	a1,[03]\(at\)
0000015c <fn\+0x15c> 9825000[03] 	lwr	a1,[03]\(at\)
00000160 <fn\+0x160> 3c010000 	lui	at,0x0
			160: R_MIPS_GOT_HI16	dg1
00000164 <fn\+0x164> 003c0821 	addu	at,at,gp
00000168 <fn\+0x168> 8c210000 	lw	at,0\(at\)
			168: R_MIPS_GOT_LO16	dg1
0000016c <fn\+0x16c> 24210038 	addiu	at,at,56
00000170 <fn\+0x170> 00250821 	addu	at,at,a1
00000174 <fn\+0x174> a825000[03] 	swl	a1,[03]\(at\)
00000178 <fn\+0x178> b825000[03] 	swr	a1,[03]\(at\)
0000017c <fn\+0x17c> 8f850000 	lw	a1,0\(gp\)
			17c: R_MIPS_GOT_PAGE	\.data\+0x3c
00000180 <fn\+0x180> 24a50000 	addiu	a1,a1,0
			180: R_MIPS_GOT_OFST	\.data\+0x3c
00000184 <fn\+0x184> 8f850000 	lw	a1,0\(gp\)
			184: R_MIPS_GOT_PAGE	\.data\+0x48
00000188 <fn\+0x188> 24a50000 	addiu	a1,a1,0
			188: R_MIPS_GOT_OFST	\.data\+0x48
0000018c <fn\+0x18c> 8f850000 	lw	a1,0\(gp\)
			18c: R_MIPS_GOT_PAGE	\.data\+0x1e27c
00000190 <fn\+0x190> 24a50000 	addiu	a1,a1,0
			190: R_MIPS_GOT_OFST	\.data\+0x1e27c
00000194 <fn\+0x194> 8f850000 	lw	a1,0\(gp\)
			194: R_MIPS_GOT_PAGE	\.data\+0x3c
00000198 <fn\+0x198> 24a50000 	addiu	a1,a1,0
			198: R_MIPS_GOT_OFST	\.data\+0x3c
0000019c <fn\+0x19c> 00b12821 	addu	a1,a1,s1
000001a0 <fn\+0x1a0> 8f850000 	lw	a1,0\(gp\)
			1a0: R_MIPS_GOT_PAGE	\.data\+0x48
000001a4 <fn\+0x1a4> 24a50000 	addiu	a1,a1,0
			1a4: R_MIPS_GOT_OFST	\.data\+0x48
000001a8 <fn\+0x1a8> 00b12821 	addu	a1,a1,s1
000001ac <fn\+0x1ac> 8f850000 	lw	a1,0\(gp\)
			1ac: R_MIPS_GOT_PAGE	\.data\+0x1e27c
000001b0 <fn\+0x1b0> 24a50000 	addiu	a1,a1,0
			1b0: R_MIPS_GOT_OFST	\.data\+0x1e27c
000001b4 <fn\+0x1b4> 00b12821 	addu	a1,a1,s1
000001b8 <fn\+0x1b8> 8f850000 	lw	a1,0\(gp\)
			1b8: R_MIPS_GOT_PAGE	\.data\+0x3c
000001bc <fn\+0x1bc> 8ca50000 	lw	a1,0\(a1\)
			1bc: R_MIPS_GOT_OFST	\.data\+0x3c
000001c0 <fn\+0x1c0> 8f850000 	lw	a1,0\(gp\)
			1c0: R_MIPS_GOT_PAGE	\.data\+0x48
000001c4 <fn\+0x1c4> 8ca50000 	lw	a1,0\(a1\)
			1c4: R_MIPS_GOT_OFST	\.data\+0x48
000001c8 <fn\+0x1c8> 8f850000 	lw	a1,0\(gp\)
			1c8: R_MIPS_GOT_PAGE	\.data\+0x3c
000001cc <fn\+0x1cc> 00b12821 	addu	a1,a1,s1
000001d0 <fn\+0x1d0> 8ca50000 	lw	a1,0\(a1\)
			1d0: R_MIPS_GOT_OFST	\.data\+0x3c
000001d4 <fn\+0x1d4> 8f850000 	lw	a1,0\(gp\)
			1d4: R_MIPS_GOT_PAGE	\.data\+0x48
000001d8 <fn\+0x1d8> 00b12821 	addu	a1,a1,s1
000001dc <fn\+0x1dc> 8ca50000 	lw	a1,0\(a1\)
			1dc: R_MIPS_GOT_OFST	\.data\+0x48
000001e0 <fn\+0x1e0> 8f810000 	lw	at,0\(gp\)
			1e0: R_MIPS_GOT_PAGE	\.data\+0x5e
000001e4 <fn\+0x1e4> 00250821 	addu	at,at,a1
000001e8 <fn\+0x1e8> 8c250000 	lw	a1,0\(at\)
			1e8: R_MIPS_GOT_OFST	\.data\+0x5e
000001ec <fn\+0x1ec> 8f810000 	lw	at,0\(gp\)
			1ec: R_MIPS_GOT_PAGE	\.data\+0x74
000001f0 <fn\+0x1f0> 00250821 	addu	at,at,a1
000001f4 <fn\+0x1f4> ac250000 	sw	a1,0\(at\)
			1f4: R_MIPS_GOT_OFST	\.data\+0x74
000001f8 <fn\+0x1f8> 8f810000 	lw	at,0\(gp\)
			1f8: R_MIPS_GOT_PAGE	\.data\+0x3c
000001fc <fn\+0x1fc> 24210000 	addiu	at,at,0
			1fc: R_MIPS_GOT_OFST	\.data\+0x3c
00000200 <fn\+0x200> 8825000[03] 	lwl	a1,[03]\(at\)
00000204 <fn\+0x204> 9825000[03] 	lwr	a1,[03]\(at\)
00000208 <fn\+0x208> 8f810000 	lw	at,0\(gp\)
			208: R_MIPS_GOT_PAGE	\.data\+0x48
0000020c <fn\+0x20c> 24210000 	addiu	at,at,0
			20c: R_MIPS_GOT_OFST	\.data\+0x48
00000210 <fn\+0x210> 8825000[03] 	lwl	a1,[03]\(at\)
00000214 <fn\+0x214> 9825000[03] 	lwr	a1,[03]\(at\)
00000218 <fn\+0x218> 8f810000 	lw	at,0\(gp\)
			218: R_MIPS_GOT_PAGE	\.data\+0x3c
0000021c <fn\+0x21c> 24210000 	addiu	at,at,0
			21c: R_MIPS_GOT_OFST	\.data\+0x3c
00000220 <fn\+0x220> 00310821 	addu	at,at,s1
00000224 <fn\+0x224> 8825000[03] 	lwl	a1,[03]\(at\)
00000228 <fn\+0x228> 9825000[03] 	lwr	a1,[03]\(at\)
0000022c <fn\+0x22c> 8f810000 	lw	at,0\(gp\)
			22c: R_MIPS_GOT_PAGE	\.data\+0x48
00000230 <fn\+0x230> 24210000 	addiu	at,at,0
			230: R_MIPS_GOT_OFST	\.data\+0x48
00000234 <fn\+0x234> 00310821 	addu	at,at,s1
00000238 <fn\+0x238> 8825000[03] 	lwl	a1,[03]\(at\)
0000023c <fn\+0x23c> 9825000[03] 	lwr	a1,[03]\(at\)
00000240 <fn\+0x240> 8f810000 	lw	at,0\(gp\)
			240: R_MIPS_GOT_PAGE	\.data\+0x5e
00000244 <fn\+0x244> 24210000 	addiu	at,at,0
			244: R_MIPS_GOT_OFST	\.data\+0x5e
00000248 <fn\+0x248> 00250821 	addu	at,at,a1
0000024c <fn\+0x24c> 8825000[03] 	lwl	a1,[03]\(at\)
00000250 <fn\+0x250> 9825000[03] 	lwr	a1,[03]\(at\)
00000254 <fn\+0x254> 8f810000 	lw	at,0\(gp\)
			254: R_MIPS_GOT_PAGE	\.data\+0x74
00000258 <fn\+0x258> 24210000 	addiu	at,at,0
			258: R_MIPS_GOT_OFST	\.data\+0x74
0000025c <fn\+0x25c> 00250821 	addu	at,at,a1
00000260 <fn\+0x260> a825000[03] 	swl	a1,[03]\(at\)
00000264 <fn\+0x264> b825000[03] 	swr	a1,[03]\(at\)
00000268 <fn\+0x268> 3c050000 	lui	a1,0x0
			268: R_MIPS_GOT_HI16	fn
0000026c <fn\+0x26c> 00bc2821 	addu	a1,a1,gp
00000270 <fn\+0x270> 8ca50000 	lw	a1,0\(a1\)
			270: R_MIPS_GOT_LO16	fn
00000274 <fn\+0x274> 8f850000 	lw	a1,0\(gp\)
			274: R_MIPS_GOT_PAGE	\.text
00000278 <fn\+0x278> 24a50000 	addiu	a1,a1,0
			278: R_MIPS_GOT_OFST	\.text
0000027c <fn\+0x27c> 3c190000 	lui	t9,0x0
			27c: R_MIPS_CALL_HI16	fn
00000280 <fn\+0x280> 033cc821 	addu	t9,t9,gp
00000284 <fn\+0x284> 8f390000 	lw	t9,0\(t9\)
			284: R_MIPS_CALL_LO16	fn
00000288 <fn\+0x288> 8f990000 	lw	t9,0\(gp\)
			288: R_MIPS_GOT_PAGE	\.text
0000028c <fn\+0x28c> 27390000 	addiu	t9,t9,0
			28c: R_MIPS_GOT_OFST	\.text
00000290 <fn\+0x290> 3c190000 	lui	t9,0x0
			290: R_MIPS_CALL_HI16	fn
00000294 <fn\+0x294> 033cc821 	addu	t9,t9,gp
00000298 <fn\+0x298> 8f390000 	lw	t9,0\(t9\)
			298: R_MIPS_CALL_LO16	fn
0000029c <fn\+0x29c> 0320f809 	jalr	t9
			29c: R_MIPS_JALR	fn
000002a0 <fn\+0x2a0> 00000000 	nop
000002a4 <fn\+0x2a4> 8f990000 	lw	t9,0\(gp\)
			2a4: R_MIPS_GOT_PAGE	\.text
000002a8 <fn\+0x2a8> 27390000 	addiu	t9,t9,0
			2a8: R_MIPS_GOT_OFST	\.text
000002ac <fn\+0x2ac> 0320f809 	jalr	t9
			2ac: R_MIPS_JALR	\.text
000002b0 <fn\+0x2b0> 00000000 	nop
000002b4 <fn\+0x2b4> 3c050000 	lui	a1,0x0
			2b4: R_MIPS_GOT_HI16	dg2
000002b8 <fn\+0x2b8> 00bc2821 	addu	a1,a1,gp
000002bc <fn\+0x2bc> 8ca50000 	lw	a1,0\(a1\)
			2bc: R_MIPS_GOT_LO16	dg2
000002c0 <fn\+0x2c0> 3c050000 	lui	a1,0x0
			2c0: R_MIPS_GOT_HI16	dg2
000002c4 <fn\+0x2c4> 00bc2821 	addu	a1,a1,gp
000002c8 <fn\+0x2c8> 8ca50000 	lw	a1,0\(a1\)
			2c8: R_MIPS_GOT_LO16	dg2
000002cc <fn\+0x2cc> 24a5000c 	addiu	a1,a1,12
000002d0 <fn\+0x2d0> 3c050000 	lui	a1,0x0
			2d0: R_MIPS_GOT_HI16	dg2
000002d4 <fn\+0x2d4> 00bc2821 	addu	a1,a1,gp
000002d8 <fn\+0x2d8> 8ca50000 	lw	a1,0\(a1\)
			2d8: R_MIPS_GOT_LO16	dg2
000002dc <fn\+0x2dc> 3c010001 	lui	at,0x1
000002e0 <fn\+0x2e0> 3421e240 	ori	at,at,0xe240
000002e4 <fn\+0x2e4> 00a12821 	addu	a1,a1,at
000002e8 <fn\+0x2e8> 3c050000 	lui	a1,0x0
			2e8: R_MIPS_GOT_HI16	dg2
000002ec <fn\+0x2ec> 00bc2821 	addu	a1,a1,gp
000002f0 <fn\+0x2f0> 8ca50000 	lw	a1,0\(a1\)
			2f0: R_MIPS_GOT_LO16	dg2
000002f4 <fn\+0x2f4> 00b12821 	addu	a1,a1,s1
000002f8 <fn\+0x2f8> 3c050000 	lui	a1,0x0
			2f8: R_MIPS_GOT_HI16	dg2
000002fc <fn\+0x2fc> 00bc2821 	addu	a1,a1,gp
00000300 <fn\+0x300> 8ca50000 	lw	a1,0\(a1\)
			300: R_MIPS_GOT_LO16	dg2
00000304 <fn\+0x304> 24a5000c 	addiu	a1,a1,12
00000308 <fn\+0x308> 00b12821 	addu	a1,a1,s1
0000030c <fn\+0x30c> 3c050000 	lui	a1,0x0
			30c: R_MIPS_GOT_HI16	dg2
00000310 <fn\+0x310> 00bc2821 	addu	a1,a1,gp
00000314 <fn\+0x314> 8ca50000 	lw	a1,0\(a1\)
			314: R_MIPS_GOT_LO16	dg2
00000318 <fn\+0x318> 3c010001 	lui	at,0x1
0000031c <fn\+0x31c> 3421e240 	ori	at,at,0xe240
00000320 <fn\+0x320> 00a12821 	addu	a1,a1,at
00000324 <fn\+0x324> 00b12821 	addu	a1,a1,s1
00000328 <fn\+0x328> 3c050000 	lui	a1,0x0
			328: R_MIPS_GOT_HI16	dg2
0000032c <fn\+0x32c> 00bc2821 	addu	a1,a1,gp
00000330 <fn\+0x330> 8ca50000 	lw	a1,0\(a1\)
			330: R_MIPS_GOT_LO16	dg2
00000334 <fn\+0x334> 8ca50000 	lw	a1,0\(a1\)
00000338 <fn\+0x338> 3c050000 	lui	a1,0x0
			338: R_MIPS_GOT_HI16	dg2
0000033c <fn\+0x33c> 00bc2821 	addu	a1,a1,gp
00000340 <fn\+0x340> 8ca50000 	lw	a1,0\(a1\)
			340: R_MIPS_GOT_LO16	dg2
00000344 <fn\+0x344> 8ca5000c 	lw	a1,12\(a1\)
00000348 <fn\+0x348> 3c050000 	lui	a1,0x0
			348: R_MIPS_GOT_HI16	dg2
0000034c <fn\+0x34c> 00bc2821 	addu	a1,a1,gp
00000350 <fn\+0x350> 8ca50000 	lw	a1,0\(a1\)
			350: R_MIPS_GOT_LO16	dg2
00000354 <fn\+0x354> 00b12821 	addu	a1,a1,s1
00000358 <fn\+0x358> 8ca50000 	lw	a1,0\(a1\)
0000035c <fn\+0x35c> 3c050000 	lui	a1,0x0
			35c: R_MIPS_GOT_HI16	dg2
00000360 <fn\+0x360> 00bc2821 	addu	a1,a1,gp
00000364 <fn\+0x364> 8ca50000 	lw	a1,0\(a1\)
			364: R_MIPS_GOT_LO16	dg2
00000368 <fn\+0x368> 00b12821 	addu	a1,a1,s1
0000036c <fn\+0x36c> 8ca5000c 	lw	a1,12\(a1\)
00000370 <fn\+0x370> 3c010000 	lui	at,0x0
			370: R_MIPS_GOT_HI16	dg2
00000374 <fn\+0x374> 003c0821 	addu	at,at,gp
00000378 <fn\+0x378> 8c210000 	lw	at,0\(at\)
			378: R_MIPS_GOT_LO16	dg2
0000037c <fn\+0x37c> 00250821 	addu	at,at,a1
00000380 <fn\+0x380> 8c250022 	lw	a1,34\(at\)
00000384 <fn\+0x384> 3c010000 	lui	at,0x0
			384: R_MIPS_GOT_HI16	dg2
00000388 <fn\+0x388> 003c0821 	addu	at,at,gp
0000038c <fn\+0x38c> 8c210000 	lw	at,0\(at\)
			38c: R_MIPS_GOT_LO16	dg2
00000390 <fn\+0x390> 00250821 	addu	at,at,a1
00000394 <fn\+0x394> ac250038 	sw	a1,56\(at\)
00000398 <fn\+0x398> 3c010000 	lui	at,0x0
			398: R_MIPS_GOT_HI16	dg2
0000039c <fn\+0x39c> 003c0821 	addu	at,at,gp
000003a0 <fn\+0x3a0> 8c210000 	lw	at,0\(at\)
			3a0: R_MIPS_GOT_LO16	dg2
000003a4 <fn\+0x3a4> 8825000[03] 	lwl	a1,[03]\(at\)
000003a8 <fn\+0x3a8> 9825000[03] 	lwr	a1,[03]\(at\)
000003ac <fn\+0x3ac> 3c010000 	lui	at,0x0
			3ac: R_MIPS_GOT_HI16	dg2
000003b0 <fn\+0x3b0> 003c0821 	addu	at,at,gp
000003b4 <fn\+0x3b4> 8c210000 	lw	at,0\(at\)
			3b4: R_MIPS_GOT_LO16	dg2
000003b8 <fn\+0x3b8> 2421000c 	addiu	at,at,12
000003bc <fn\+0x3bc> 8825000[03] 	lwl	a1,[03]\(at\)
000003c0 <fn\+0x3c0> 9825000[03] 	lwr	a1,[03]\(at\)
000003c4 <fn\+0x3c4> 3c010000 	lui	at,0x0
			3c4: R_MIPS_GOT_HI16	dg2
000003c8 <fn\+0x3c8> 003c0821 	addu	at,at,gp
000003cc <fn\+0x3cc> 8c210000 	lw	at,0\(at\)
			3cc: R_MIPS_GOT_LO16	dg2
000003d0 <fn\+0x3d0> 00310821 	addu	at,at,s1
000003d4 <fn\+0x3d4> 8825000[03] 	lwl	a1,[03]\(at\)
000003d8 <fn\+0x3d8> 9825000[03] 	lwr	a1,[03]\(at\)
000003dc <fn\+0x3dc> 3c010000 	lui	at,0x0
			3dc: R_MIPS_GOT_HI16	dg2
000003e0 <fn\+0x3e0> 003c0821 	addu	at,at,gp
000003e4 <fn\+0x3e4> 8c210000 	lw	at,0\(at\)
			3e4: R_MIPS_GOT_LO16	dg2
000003e8 <fn\+0x3e8> 2421000c 	addiu	at,at,12
000003ec <fn\+0x3ec> 00310821 	addu	at,at,s1
000003f0 <fn\+0x3f0> 8825000[03] 	lwl	a1,[03]\(at\)
000003f4 <fn\+0x3f4> 9825000[03] 	lwr	a1,[03]\(at\)
000003f8 <fn\+0x3f8> 3c010000 	lui	at,0x0
			3f8: R_MIPS_GOT_HI16	dg2
000003fc <fn\+0x3fc> 003c0821 	addu	at,at,gp
00000400 <fn\+0x400> 8c210000 	lw	at,0\(at\)
			400: R_MIPS_GOT_LO16	dg2
00000404 <fn\+0x404> 24210022 	addiu	at,at,34
00000408 <fn\+0x408> 00250821 	addu	at,at,a1
0000040c <fn\+0x40c> 8825000[03] 	lwl	a1,[03]\(at\)
00000410 <fn\+0x410> 9825000[03] 	lwr	a1,[03]\(at\)
00000414 <fn\+0x414> 3c010000 	lui	at,0x0
			414: R_MIPS_GOT_HI16	dg2
00000418 <fn\+0x418> 003c0821 	addu	at,at,gp
0000041c <fn\+0x41c> 8c210000 	lw	at,0\(at\)
			41c: R_MIPS_GOT_LO16	dg2
00000420 <fn\+0x420> 24210038 	addiu	at,at,56
00000424 <fn\+0x424> 00250821 	addu	at,at,a1
00000428 <fn\+0x428> a825000[03] 	swl	a1,[03]\(at\)
0000042c <fn\+0x42c> b825000[03] 	swr	a1,[03]\(at\)
00000430 <fn\+0x430> 8f850000 	lw	a1,0\(gp\)
			430: R_MIPS_GOT_PAGE	\.data\+0xb4
00000434 <fn\+0x434> 24a50000 	addiu	a1,a1,0
			434: R_MIPS_GOT_OFST	\.data\+0xb4
00000438 <fn\+0x438> 8f850000 	lw	a1,0\(gp\)
			438: R_MIPS_GOT_PAGE	\.data\+0xc0
0000043c <fn\+0x43c> 24a50000 	addiu	a1,a1,0
			43c: R_MIPS_GOT_OFST	\.data\+0xc0
00000440 <fn\+0x440> 8f850000 	lw	a1,0\(gp\)
			440: R_MIPS_GOT_PAGE	\.data\+0x1e2f4
00000444 <fn\+0x444> 24a50000 	addiu	a1,a1,0
			444: R_MIPS_GOT_OFST	\.data\+0x1e2f4
00000448 <fn\+0x448> 8f850000 	lw	a1,0\(gp\)
			448: R_MIPS_GOT_PAGE	\.data\+0xb4
0000044c <fn\+0x44c> 24a50000 	addiu	a1,a1,0
			44c: R_MIPS_GOT_OFST	\.data\+0xb4
00000450 <fn\+0x450> 00b12821 	addu	a1,a1,s1
00000454 <fn\+0x454> 8f850000 	lw	a1,0\(gp\)
			454: R_MIPS_GOT_PAGE	\.data\+0xc0
00000458 <fn\+0x458> 24a50000 	addiu	a1,a1,0
			458: R_MIPS_GOT_OFST	\.data\+0xc0
0000045c <fn\+0x45c> 00b12821 	addu	a1,a1,s1
00000460 <fn\+0x460> 8f850000 	lw	a1,0\(gp\)
			460: R_MIPS_GOT_PAGE	\.data\+0x1e2f4
00000464 <fn\+0x464> 24a50000 	addiu	a1,a1,0
			464: R_MIPS_GOT_OFST	\.data\+0x1e2f4
00000468 <fn\+0x468> 00b12821 	addu	a1,a1,s1
0000046c <fn\+0x46c> 8f850000 	lw	a1,0\(gp\)
			46c: R_MIPS_GOT_PAGE	\.data\+0xb4
00000470 <fn\+0x470> 8ca50000 	lw	a1,0\(a1\)
			470: R_MIPS_GOT_OFST	\.data\+0xb4
00000474 <fn\+0x474> 8f850000 	lw	a1,0\(gp\)
			474: R_MIPS_GOT_PAGE	\.data\+0xc0
00000478 <fn\+0x478> 8ca50000 	lw	a1,0\(a1\)
			478: R_MIPS_GOT_OFST	\.data\+0xc0
0000047c <fn\+0x47c> 8f850000 	lw	a1,0\(gp\)
			47c: R_MIPS_GOT_PAGE	\.data\+0xb4
00000480 <fn\+0x480> 00b12821 	addu	a1,a1,s1
00000484 <fn\+0x484> 8ca50000 	lw	a1,0\(a1\)
			484: R_MIPS_GOT_OFST	\.data\+0xb4
00000488 <fn\+0x488> 8f850000 	lw	a1,0\(gp\)
			488: R_MIPS_GOT_PAGE	\.data\+0xc0
0000048c <fn\+0x48c> 00b12821 	addu	a1,a1,s1
00000490 <fn\+0x490> 8ca50000 	lw	a1,0\(a1\)
			490: R_MIPS_GOT_OFST	\.data\+0xc0
00000494 <fn\+0x494> 8f810000 	lw	at,0\(gp\)
			494: R_MIPS_GOT_PAGE	\.data\+0xd6
00000498 <fn\+0x498> 00250821 	addu	at,at,a1
0000049c <fn\+0x49c> 8c250000 	lw	a1,0\(at\)
			49c: R_MIPS_GOT_OFST	\.data\+0xd6
000004a0 <fn\+0x4a0> 8f810000 	lw	at,0\(gp\)
			4a0: R_MIPS_GOT_PAGE	\.data\+0xec
000004a4 <fn\+0x4a4> 00250821 	addu	at,at,a1
000004a8 <fn\+0x4a8> ac250000 	sw	a1,0\(at\)
			4a8: R_MIPS_GOT_OFST	\.data\+0xec
000004ac <fn\+0x4ac> 8f810000 	lw	at,0\(gp\)
			4ac: R_MIPS_GOT_PAGE	\.data\+0xb4
000004b0 <fn\+0x4b0> 24210000 	addiu	at,at,0
			4b0: R_MIPS_GOT_OFST	\.data\+0xb4
000004b4 <fn\+0x4b4> 8825000[03] 	lwl	a1,[03]\(at\)
000004b8 <fn\+0x4b8> 9825000[03] 	lwr	a1,[03]\(at\)
000004bc <fn\+0x4bc> 8f810000 	lw	at,0\(gp\)
			4bc: R_MIPS_GOT_PAGE	\.data\+0xc0
000004c0 <fn\+0x4c0> 24210000 	addiu	at,at,0
			4c0: R_MIPS_GOT_OFST	\.data\+0xc0
000004c4 <fn\+0x4c4> 8825000[03] 	lwl	a1,[03]\(at\)
000004c8 <fn\+0x4c8> 9825000[03] 	lwr	a1,[03]\(at\)
000004cc <fn\+0x4cc> 8f810000 	lw	at,0\(gp\)
			4cc: R_MIPS_GOT_PAGE	\.data\+0xb4
000004d0 <fn\+0x4d0> 24210000 	addiu	at,at,0
			4d0: R_MIPS_GOT_OFST	\.data\+0xb4
000004d4 <fn\+0x4d4> 00310821 	addu	at,at,s1
000004d8 <fn\+0x4d8> 8825000[03] 	lwl	a1,[03]\(at\)
000004dc <fn\+0x4dc> 9825000[03] 	lwr	a1,[03]\(at\)
000004e0 <fn\+0x4e0> 8f810000 	lw	at,0\(gp\)
			4e0: R_MIPS_GOT_PAGE	\.data\+0xc0
000004e4 <fn\+0x4e4> 24210000 	addiu	at,at,0
			4e4: R_MIPS_GOT_OFST	\.data\+0xc0
000004e8 <fn\+0x4e8> 00310821 	addu	at,at,s1
000004ec <fn\+0x4ec> 8825000[03] 	lwl	a1,[03]\(at\)
000004f0 <fn\+0x4f0> 9825000[03] 	lwr	a1,[03]\(at\)
000004f4 <fn\+0x4f4> 8f810000 	lw	at,0\(gp\)
			4f4: R_MIPS_GOT_PAGE	\.data\+0xd6
000004f8 <fn\+0x4f8> 24210000 	addiu	at,at,0
			4f8: R_MIPS_GOT_OFST	\.data\+0xd6
000004fc <fn\+0x4fc> 00250821 	addu	at,at,a1
00000500 <fn\+0x500> 8825000[03] 	lwl	a1,[03]\(at\)
00000504 <fn\+0x504> 9825000[03] 	lwr	a1,[03]\(at\)
00000508 <fn\+0x508> 8f810000 	lw	at,0\(gp\)
			508: R_MIPS_GOT_PAGE	\.data\+0xec
0000050c <fn\+0x50c> 24210000 	addiu	at,at,0
			50c: R_MIPS_GOT_OFST	\.data\+0xec
00000510 <fn\+0x510> 00250821 	addu	at,at,a1
00000514 <fn\+0x514> a825000[03] 	swl	a1,[03]\(at\)
00000518 <fn\+0x518> b825000[03] 	swr	a1,[03]\(at\)
0000051c <fn\+0x51c> 3c050000 	lui	a1,0x0
			51c: R_MIPS_GOT_HI16	fn2
00000520 <fn\+0x520> 00bc2821 	addu	a1,a1,gp
00000524 <fn\+0x524> 8ca50000 	lw	a1,0\(a1\)
			524: R_MIPS_GOT_LO16	fn2
00000528 <fn\+0x528> 8f850000 	lw	a1,0\(gp\)
			528: R_MIPS_GOT_PAGE	\.text\+0x600
0000052c <fn\+0x52c> 24a50000 	addiu	a1,a1,0
			52c: R_MIPS_GOT_OFST	\.text\+0x600
00000530 <fn\+0x530> 3c190000 	lui	t9,0x0
			530: R_MIPS_CALL_HI16	fn2
00000534 <fn\+0x534> 033cc821 	addu	t9,t9,gp
00000538 <fn\+0x538> 8f390000 	lw	t9,0\(t9\)
			538: R_MIPS_CALL_LO16	fn2
0000053c <fn\+0x53c> 8f990000 	lw	t9,0\(gp\)
			53c: R_MIPS_GOT_PAGE	\.text\+0x600
00000540 <fn\+0x540> 27390000 	addiu	t9,t9,0
			540: R_MIPS_GOT_OFST	\.text\+0x600
00000544 <fn\+0x544> 3c190000 	lui	t9,0x0
			544: R_MIPS_CALL_HI16	fn2
00000548 <fn\+0x548> 033cc821 	addu	t9,t9,gp
0000054c <fn\+0x54c> 8f390000 	lw	t9,0\(t9\)
			54c: R_MIPS_CALL_LO16	fn2
00000550 <fn\+0x550> 0320f809 	jalr	t9
			550: R_MIPS_JALR	fn2
00000554 <fn\+0x554> 00000000 	nop
00000558 <fn\+0x558> 8f990000 	lw	t9,0\(gp\)
			558: R_MIPS_GOT_PAGE	\.text\+0x600
0000055c <fn\+0x55c> 27390000 	addiu	t9,t9,0
			55c: R_MIPS_GOT_OFST	\.text\+0x600
00000560 <fn\+0x560> 0320f809 	jalr	t9
			560: R_MIPS_JALR	\.text\+0x600
00000564 <fn\+0x564> 00000000 	nop
00000568 <fn\+0x568> 3c050000 	lui	a1,0x0
			568: R_MIPS_GOT_HI16	dg1
0000056c <fn\+0x56c> 00bc2821 	addu	a1,a1,gp
00000570 <fn\+0x570> 8ca50000 	lw	a1,0\(a1\)
			570: R_MIPS_GOT_LO16	dg1
00000574 <fn\+0x574> 1000fea2 	b	00000000 <fn>
00000578 <fn\+0x578> 00000000 	nop
0000057c <fn\+0x57c> 3c050000 	lui	a1,0x0
			57c: R_MIPS_GOT_HI16	dg2
00000580 <fn\+0x580> 00bc2821 	addu	a1,a1,gp
00000584 <fn\+0x584> 8ca50000 	lw	a1,0\(a1\)
			584: R_MIPS_GOT_LO16	dg2
00000588 <fn\+0x588> 8ca50000 	lw	a1,0\(a1\)
0000058c <fn\+0x58c> 1000001c 	b	00000600 <fn2>
00000590 <fn\+0x590> 00000000 	nop
00000594 <fn\+0x594> 8f850000 	lw	a1,0\(gp\)
			594: R_MIPS_GOT_PAGE	\.data\+0x3c
00000598 <fn\+0x598> 24a50000 	addiu	a1,a1,0
			598: R_MIPS_GOT_OFST	\.data\+0x3c
0000059c <fn\+0x59c> 1000fe98 	b	00000000 <fn>
000005a0 <fn\+0x5a0> 00000000 	nop
000005a4 <fn\+0x5a4> 8f850000 	lw	a1,0\(gp\)
			5a4: R_MIPS_GOT_PAGE	\.data\+0xc0
000005a8 <fn\+0x5a8> 24a50000 	addiu	a1,a1,0
			5a8: R_MIPS_GOT_OFST	\.data\+0xc0
000005ac <fn\+0x5ac> 10000014 	b	00000600 <fn2>
000005b0 <fn\+0x5b0> 00000000 	nop
000005b4 <fn\+0x5b4> 8f850000 	lw	a1,0\(gp\)
			5b4: R_MIPS_GOT_PAGE	\.data\+0x1e27c
000005b8 <fn\+0x5b8> 24a50000 	addiu	a1,a1,0
			5b8: R_MIPS_GOT_OFST	\.data\+0x1e27c
000005bc <fn\+0x5bc> 1000fe90 	b	00000000 <fn>
000005c0 <fn\+0x5c0> 00000000 	nop
000005c4 <fn\+0x5c4> 8f850000 	lw	a1,0\(gp\)
			5c4: R_MIPS_GOT_PAGE	\.data\+0xb4
000005c8 <fn\+0x5c8> 8ca50000 	lw	a1,0\(a1\)
			5c8: R_MIPS_GOT_OFST	\.data\+0xb4
000005cc <fn\+0x5cc> 1000000c 	b	00000600 <fn2>
000005d0 <fn\+0x5d0> 00000000 	nop
000005d4 <fn\+0x5d4> 8f850000 	lw	a1,0\(gp\)
			5d4: R_MIPS_GOT_PAGE	\.data\+0x48
000005d8 <fn\+0x5d8> 8ca50000 	lw	a1,0\(a1\)
			5d8: R_MIPS_GOT_OFST	\.data\+0x48
000005dc <fn\+0x5dc> 1000fe88 	b	00000000 <fn>
000005e0 <fn\+0x5e0> 00000000 	nop
000005e4 <fn\+0x5e4> 8f810000 	lw	at,0\(gp\)
			5e4: R_MIPS_GOT_PAGE	\.data\+0xd6
000005e8 <fn\+0x5e8> 00250821 	addu	at,at,a1
000005ec <fn\+0x5ec> 8c250000 	lw	a1,0\(at\)
			5ec: R_MIPS_GOT_OFST	\.data\+0xd6
000005f0 <fn\+0x5f0> 10000003 	b	00000600 <fn2>
000005f4 <fn\+0x5f4> 00000000 	nop
	\.\.\.
