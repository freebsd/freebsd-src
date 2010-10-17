#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS ELF got reloc n32
#as: -n32 -KPIC

.*: +file format elf32-n.*mips.*

Disassembly of section \.text:
00000000 <fn> 8f850000 	lw	a1,0\(gp\)
			0: R_MIPS_GOT_DISP	dg1
00000004 <fn\+0x4> 8f850000 	lw	a1,0\(gp\)
			4: R_MIPS_GOT_DISP	dg1
00000008 <fn\+0x8> 24a5000c 	addiu	a1,a1,12
0000000c <fn\+0xc> 8f850000 	lw	a1,0\(gp\)
			c: R_MIPS_GOT_DISP	dg1
00000010 <fn\+0x10> 3c010001 	lui	at,0x1
00000014 <fn\+0x14> 3421e240 	ori	at,at,0xe240
00000018 <fn\+0x18> 00a12821 	addu	a1,a1,at
0000001c <fn\+0x1c> 8f850000 	lw	a1,0\(gp\)
			1c: R_MIPS_GOT_DISP	dg1
00000020 <fn\+0x20> 00b12821 	addu	a1,a1,s1
00000024 <fn\+0x24> 8f850000 	lw	a1,0\(gp\)
			24: R_MIPS_GOT_DISP	dg1
00000028 <fn\+0x28> 24a5000c 	addiu	a1,a1,12
0000002c <fn\+0x2c> 00b12821 	addu	a1,a1,s1
00000030 <fn\+0x30> 8f850000 	lw	a1,0\(gp\)
			30: R_MIPS_GOT_DISP	dg1
00000034 <fn\+0x34> 3c010001 	lui	at,0x1
00000038 <fn\+0x38> 3421e240 	ori	at,at,0xe240
0000003c <fn\+0x3c> 00a12821 	addu	a1,a1,at
00000040 <fn\+0x40> 00b12821 	addu	a1,a1,s1
00000044 <fn\+0x44> 8f850000 	lw	a1,0\(gp\)
			44: R_MIPS_GOT_PAGE	dg1
00000048 <fn\+0x48> 8ca50000 	lw	a1,0\(a1\)
			48: R_MIPS_GOT_OFST	dg1
0000004c <fn\+0x4c> 8f850000 	lw	a1,0\(gp\)
			4c: R_MIPS_GOT_PAGE	dg1\+0xc
00000050 <fn\+0x50> 8ca50000 	lw	a1,0\(a1\)
			50: R_MIPS_GOT_OFST	dg1\+0xc
00000054 <fn\+0x54> 8f850000 	lw	a1,0\(gp\)
			54: R_MIPS_GOT_PAGE	dg1
00000058 <fn\+0x58> 00b12821 	addu	a1,a1,s1
0000005c <fn\+0x5c> 8ca50000 	lw	a1,0\(a1\)
			5c: R_MIPS_GOT_OFST	dg1
00000060 <fn\+0x60> 8f850000 	lw	a1,0\(gp\)
			60: R_MIPS_GOT_PAGE	dg1\+0xc
00000064 <fn\+0x64> 00b12821 	addu	a1,a1,s1
00000068 <fn\+0x68> 8ca50000 	lw	a1,0\(a1\)
			68: R_MIPS_GOT_OFST	dg1\+0xc
0000006c <fn\+0x6c> 8f810000 	lw	at,0\(gp\)
			6c: R_MIPS_GOT_PAGE	dg1\+0x22
00000070 <fn\+0x70> 00250821 	addu	at,at,a1
00000074 <fn\+0x74> 8c250000 	lw	a1,0\(at\)
			74: R_MIPS_GOT_OFST	dg1\+0x22
00000078 <fn\+0x78> 8f810000 	lw	at,0\(gp\)
			78: R_MIPS_GOT_PAGE	dg1\+0x38
0000007c <fn\+0x7c> 00250821 	addu	at,at,a1
00000080 <fn\+0x80> ac250000 	sw	a1,0\(at\)
			80: R_MIPS_GOT_OFST	dg1\+0x38
00000084 <fn\+0x84> 8f810000 	lw	at,0\(gp\)
			84: R_MIPS_GOT_DISP	dg1
00000088 <fn\+0x88> 8825000[03] 	lwl	a1,[03]\(at\)
0000008c <fn\+0x8c> 9825000[03] 	lwr	a1,[03]\(at\)
00000090 <fn\+0x90> 8f810000 	lw	at,0\(gp\)
			90: R_MIPS_GOT_DISP	dg1
00000094 <fn\+0x94> 2421000c 	addiu	at,at,12
00000098 <fn\+0x98> 8825000[03] 	lwl	a1,[03]\(at\)
0000009c <fn\+0x9c> 9825000[03] 	lwr	a1,[03]\(at\)
000000a0 <fn\+0xa0> 8f810000 	lw	at,0\(gp\)
			a0: R_MIPS_GOT_DISP	dg1
000000a4 <fn\+0xa4> 00310821 	addu	at,at,s1
000000a8 <fn\+0xa8> 8825000[03] 	lwl	a1,[03]\(at\)
000000ac <fn\+0xac> 9825000[03] 	lwr	a1,[03]\(at\)
000000b0 <fn\+0xb0> 8f810000 	lw	at,0\(gp\)
			b0: R_MIPS_GOT_DISP	dg1
000000b4 <fn\+0xb4> 2421000c 	addiu	at,at,12
000000b8 <fn\+0xb8> 00310821 	addu	at,at,s1
000000bc <fn\+0xbc> 8825000[03] 	lwl	a1,[03]\(at\)
000000c0 <fn\+0xc0> 9825000[03] 	lwr	a1,[03]\(at\)
000000c4 <fn\+0xc4> 8f810000 	lw	at,0\(gp\)
			c4: R_MIPS_GOT_DISP	dg1
000000c8 <fn\+0xc8> 24210022 	addiu	at,at,34
000000cc <fn\+0xcc> 00250821 	addu	at,at,a1
000000d0 <fn\+0xd0> 8825000[03] 	lwl	a1,[03]\(at\)
000000d4 <fn\+0xd4> 9825000[03] 	lwr	a1,[03]\(at\)
000000d8 <fn\+0xd8> 8f810000 	lw	at,0\(gp\)
			d8: R_MIPS_GOT_DISP	dg1
000000dc <fn\+0xdc> 24210038 	addiu	at,at,56
000000e0 <fn\+0xe0> 00250821 	addu	at,at,a1
000000e4 <fn\+0xe4> a825000[03] 	swl	a1,[03]\(at\)
000000e8 <fn\+0xe8> b825000[03] 	swr	a1,[03]\(at\)
000000ec <fn\+0xec> 8f850000 	lw	a1,0\(gp\)
			ec: R_MIPS_GOT_DISP	\.data\+0x3c
000000f0 <fn\+0xf0> 8f850000 	lw	a1,0\(gp\)
			f0: R_MIPS_GOT_DISP	\.data\+0x48
000000f4 <fn\+0xf4> 8f850000 	lw	a1,0\(gp\)
			f4: R_MIPS_GOT_DISP	\.data\+0x1e27c
000000f8 <fn\+0xf8> 8f850000 	lw	a1,0\(gp\)
			f8: R_MIPS_GOT_DISP	\.data\+0x3c
000000fc <fn\+0xfc> 00b12821 	addu	a1,a1,s1
00000100 <fn\+0x100> 8f850000 	lw	a1,0\(gp\)
			100: R_MIPS_GOT_DISP	\.data\+0x48
00000104 <fn\+0x104> 00b12821 	addu	a1,a1,s1
00000108 <fn\+0x108> 8f850000 	lw	a1,0\(gp\)
			108: R_MIPS_GOT_DISP	\.data\+0x1e27c
0000010c <fn\+0x10c> 00b12821 	addu	a1,a1,s1
00000110 <fn\+0x110> 8f850000 	lw	a1,0\(gp\)
			110: R_MIPS_GOT_PAGE	\.data\+0x3c
00000114 <fn\+0x114> 8ca50000 	lw	a1,0\(a1\)
			114: R_MIPS_GOT_OFST	\.data\+0x3c
00000118 <fn\+0x118> 8f850000 	lw	a1,0\(gp\)
			118: R_MIPS_GOT_PAGE	\.data\+0x48
0000011c <fn\+0x11c> 8ca50000 	lw	a1,0\(a1\)
			11c: R_MIPS_GOT_OFST	\.data\+0x48
00000120 <fn\+0x120> 8f850000 	lw	a1,0\(gp\)
			120: R_MIPS_GOT_PAGE	\.data\+0x3c
00000124 <fn\+0x124> 00b12821 	addu	a1,a1,s1
00000128 <fn\+0x128> 8ca50000 	lw	a1,0\(a1\)
			128: R_MIPS_GOT_OFST	\.data\+0x3c
0000012c <fn\+0x12c> 8f850000 	lw	a1,0\(gp\)
			12c: R_MIPS_GOT_PAGE	\.data\+0x48
00000130 <fn\+0x130> 00b12821 	addu	a1,a1,s1
00000134 <fn\+0x134> 8ca50000 	lw	a1,0\(a1\)
			134: R_MIPS_GOT_OFST	\.data\+0x48
00000138 <fn\+0x138> 8f810000 	lw	at,0\(gp\)
			138: R_MIPS_GOT_PAGE	\.data\+0x5e
0000013c <fn\+0x13c> 00250821 	addu	at,at,a1
00000140 <fn\+0x140> 8c250000 	lw	a1,0\(at\)
			140: R_MIPS_GOT_OFST	\.data\+0x5e
00000144 <fn\+0x144> 8f810000 	lw	at,0\(gp\)
			144: R_MIPS_GOT_PAGE	\.data\+0x74
00000148 <fn\+0x148> 00250821 	addu	at,at,a1
0000014c <fn\+0x14c> ac250000 	sw	a1,0\(at\)
			14c: R_MIPS_GOT_OFST	\.data\+0x74
00000150 <fn\+0x150> 8f810000 	lw	at,0\(gp\)
			150: R_MIPS_GOT_DISP	\.data\+0x3c
00000154 <fn\+0x154> 8825000[03] 	lwl	a1,[03]\(at\)
00000158 <fn\+0x158> 9825000[03] 	lwr	a1,[03]\(at\)
0000015c <fn\+0x15c> 8f810000 	lw	at,0\(gp\)
			15c: R_MIPS_GOT_DISP	\.data\+0x48
00000160 <fn\+0x160> 8825000[03] 	lwl	a1,[03]\(at\)
00000164 <fn\+0x164> 9825000[03] 	lwr	a1,[03]\(at\)
00000168 <fn\+0x168> 8f810000 	lw	at,0\(gp\)
			168: R_MIPS_GOT_DISP	\.data\+0x3c
0000016c <fn\+0x16c> 00310821 	addu	at,at,s1
00000170 <fn\+0x170> 8825000[03] 	lwl	a1,[03]\(at\)
00000174 <fn\+0x174> 9825000[03] 	lwr	a1,[03]\(at\)
00000178 <fn\+0x178> 8f810000 	lw	at,0\(gp\)
			178: R_MIPS_GOT_DISP	\.data\+0x48
0000017c <fn\+0x17c> 00310821 	addu	at,at,s1
00000180 <fn\+0x180> 8825000[03] 	lwl	a1,[03]\(at\)
00000184 <fn\+0x184> 9825000[03] 	lwr	a1,[03]\(at\)
00000188 <fn\+0x188> 8f810000 	lw	at,0\(gp\)
			188: R_MIPS_GOT_DISP	\.data\+0x5e
0000018c <fn\+0x18c> 00250821 	addu	at,at,a1
00000190 <fn\+0x190> 8825000[03] 	lwl	a1,[03]\(at\)
00000194 <fn\+0x194> 9825000[03] 	lwr	a1,[03]\(at\)
00000198 <fn\+0x198> 8f810000 	lw	at,0\(gp\)
			198: R_MIPS_GOT_DISP	\.data\+0x74
0000019c <fn\+0x19c> 00250821 	addu	at,at,a1
000001a0 <fn\+0x1a0> a825000[03] 	swl	a1,[03]\(at\)
000001a4 <fn\+0x1a4> b825000[03] 	swr	a1,[03]\(at\)
000001a8 <fn\+0x1a8> 8f850000 	lw	a1,0\(gp\)
			1a8: R_MIPS_GOT_DISP	fn
000001ac <fn\+0x1ac> 8f850000 	lw	a1,0\(gp\)
			1ac: R_MIPS_GOT_DISP	\.text
000001b0 <fn\+0x1b0> 8f990000 	lw	t9,0\(gp\)
			1b0: R_MIPS_CALL16	fn
000001b4 <fn\+0x1b4> 8f990000 	lw	t9,0\(gp\)
			1b4: R_MIPS_GOT_DISP	\.text
000001b8 <fn\+0x1b8> 8f990000 	lw	t9,0\(gp\)
			1b8: R_MIPS_CALL16	fn
000001bc <fn\+0x1bc> 0320f809 	jalr	t9
			1bc: R_MIPS_JALR	fn
000001c0 <fn\+0x1c0> 00000000 	nop
000001c4 <fn\+0x1c4> 8f990000 	lw	t9,0\(gp\)
			1c4: R_MIPS_GOT_DISP	\.text
000001c8 <fn\+0x1c8> 0320f809 	jalr	t9
			1c8: R_MIPS_JALR	\.text
000001cc <fn\+0x1cc> 00000000 	nop
000001d0 <fn\+0x1d0> 8f850000 	lw	a1,0\(gp\)
			1d0: R_MIPS_GOT_DISP	dg2
000001d4 <fn\+0x1d4> 8f850000 	lw	a1,0\(gp\)
			1d4: R_MIPS_GOT_DISP	dg2
000001d8 <fn\+0x1d8> 24a5000c 	addiu	a1,a1,12
000001dc <fn\+0x1dc> 8f850000 	lw	a1,0\(gp\)
			1dc: R_MIPS_GOT_DISP	dg2
000001e0 <fn\+0x1e0> 3c010001 	lui	at,0x1
000001e4 <fn\+0x1e4> 3421e240 	ori	at,at,0xe240
000001e8 <fn\+0x1e8> 00a12821 	addu	a1,a1,at
000001ec <fn\+0x1ec> 8f850000 	lw	a1,0\(gp\)
			1ec: R_MIPS_GOT_DISP	dg2
000001f0 <fn\+0x1f0> 00b12821 	addu	a1,a1,s1
000001f4 <fn\+0x1f4> 8f850000 	lw	a1,0\(gp\)
			1f4: R_MIPS_GOT_DISP	dg2
000001f8 <fn\+0x1f8> 24a5000c 	addiu	a1,a1,12
000001fc <fn\+0x1fc> 00b12821 	addu	a1,a1,s1
00000200 <fn\+0x200> 8f850000 	lw	a1,0\(gp\)
			200: R_MIPS_GOT_DISP	dg2
00000204 <fn\+0x204> 3c010001 	lui	at,0x1
00000208 <fn\+0x208> 3421e240 	ori	at,at,0xe240
0000020c <fn\+0x20c> 00a12821 	addu	a1,a1,at
00000210 <fn\+0x210> 00b12821 	addu	a1,a1,s1
00000214 <fn\+0x214> 8f850000 	lw	a1,0\(gp\)
			214: R_MIPS_GOT_PAGE	dg2
00000218 <fn\+0x218> 8ca50000 	lw	a1,0\(a1\)
			218: R_MIPS_GOT_OFST	dg2
0000021c <fn\+0x21c> 8f850000 	lw	a1,0\(gp\)
			21c: R_MIPS_GOT_PAGE	dg2\+0xc
00000220 <fn\+0x220> 8ca50000 	lw	a1,0\(a1\)
			220: R_MIPS_GOT_OFST	dg2\+0xc
00000224 <fn\+0x224> 8f850000 	lw	a1,0\(gp\)
			224: R_MIPS_GOT_PAGE	dg2
00000228 <fn\+0x228> 00b12821 	addu	a1,a1,s1
0000022c <fn\+0x22c> 8ca50000 	lw	a1,0\(a1\)
			22c: R_MIPS_GOT_OFST	dg2
00000230 <fn\+0x230> 8f850000 	lw	a1,0\(gp\)
			230: R_MIPS_GOT_PAGE	dg2\+0xc
00000234 <fn\+0x234> 00b12821 	addu	a1,a1,s1
00000238 <fn\+0x238> 8ca50000 	lw	a1,0\(a1\)
			238: R_MIPS_GOT_OFST	dg2\+0xc
0000023c <fn\+0x23c> 8f810000 	lw	at,0\(gp\)
			23c: R_MIPS_GOT_PAGE	dg2\+0x22
00000240 <fn\+0x240> 00250821 	addu	at,at,a1
00000244 <fn\+0x244> 8c250000 	lw	a1,0\(at\)
			244: R_MIPS_GOT_OFST	dg2\+0x22
00000248 <fn\+0x248> 8f810000 	lw	at,0\(gp\)
			248: R_MIPS_GOT_PAGE	dg2\+0x38
0000024c <fn\+0x24c> 00250821 	addu	at,at,a1
00000250 <fn\+0x250> ac250000 	sw	a1,0\(at\)
			250: R_MIPS_GOT_OFST	dg2\+0x38
00000254 <fn\+0x254> 8f810000 	lw	at,0\(gp\)
			254: R_MIPS_GOT_DISP	dg2
00000258 <fn\+0x258> 8825000[03] 	lwl	a1,[03]\(at\)
0000025c <fn\+0x25c> 9825000[03] 	lwr	a1,[03]\(at\)
00000260 <fn\+0x260> 8f810000 	lw	at,0\(gp\)
			260: R_MIPS_GOT_DISP	dg2
00000264 <fn\+0x264> 2421000c 	addiu	at,at,12
00000268 <fn\+0x268> 8825000[03] 	lwl	a1,[03]\(at\)
0000026c <fn\+0x26c> 9825000[03] 	lwr	a1,[03]\(at\)
00000270 <fn\+0x270> 8f810000 	lw	at,0\(gp\)
			270: R_MIPS_GOT_DISP	dg2
00000274 <fn\+0x274> 00310821 	addu	at,at,s1
00000278 <fn\+0x278> 8825000[03] 	lwl	a1,[03]\(at\)
0000027c <fn\+0x27c> 9825000[03] 	lwr	a1,[03]\(at\)
00000280 <fn\+0x280> 8f810000 	lw	at,0\(gp\)
			280: R_MIPS_GOT_DISP	dg2
00000284 <fn\+0x284> 2421000c 	addiu	at,at,12
00000288 <fn\+0x288> 00310821 	addu	at,at,s1
0000028c <fn\+0x28c> 8825000[03] 	lwl	a1,[03]\(at\)
00000290 <fn\+0x290> 9825000[03] 	lwr	a1,[03]\(at\)
00000294 <fn\+0x294> 8f810000 	lw	at,0\(gp\)
			294: R_MIPS_GOT_DISP	dg2
00000298 <fn\+0x298> 24210022 	addiu	at,at,34
0000029c <fn\+0x29c> 00250821 	addu	at,at,a1
000002a0 <fn\+0x2a0> 8825000[03] 	lwl	a1,[03]\(at\)
000002a4 <fn\+0x2a4> 9825000[03] 	lwr	a1,[03]\(at\)
000002a8 <fn\+0x2a8> 8f810000 	lw	at,0\(gp\)
			2a8: R_MIPS_GOT_DISP	dg2
000002ac <fn\+0x2ac> 24210038 	addiu	at,at,56
000002b0 <fn\+0x2b0> 00250821 	addu	at,at,a1
000002b4 <fn\+0x2b4> a825000[03] 	swl	a1,[03]\(at\)
000002b8 <fn\+0x2b8> b825000[03] 	swr	a1,[03]\(at\)
000002bc <fn\+0x2bc> 8f850000 	lw	a1,0\(gp\)
			2bc: R_MIPS_GOT_DISP	\.data\+0xb4
000002c0 <fn\+0x2c0> 8f850000 	lw	a1,0\(gp\)
			2c0: R_MIPS_GOT_DISP	\.data\+0xc0
000002c4 <fn\+0x2c4> 8f850000 	lw	a1,0\(gp\)
			2c4: R_MIPS_GOT_DISP	\.data\+0x1e2f4
000002c8 <fn\+0x2c8> 8f850000 	lw	a1,0\(gp\)
			2c8: R_MIPS_GOT_DISP	\.data\+0xb4
000002cc <fn\+0x2cc> 00b12821 	addu	a1,a1,s1
000002d0 <fn\+0x2d0> 8f850000 	lw	a1,0\(gp\)
			2d0: R_MIPS_GOT_DISP	\.data\+0xc0
000002d4 <fn\+0x2d4> 00b12821 	addu	a1,a1,s1
000002d8 <fn\+0x2d8> 8f850000 	lw	a1,0\(gp\)
			2d8: R_MIPS_GOT_DISP	\.data\+0x1e2f4
000002dc <fn\+0x2dc> 00b12821 	addu	a1,a1,s1
000002e0 <fn\+0x2e0> 8f850000 	lw	a1,0\(gp\)
			2e0: R_MIPS_GOT_PAGE	\.data\+0xb4
000002e4 <fn\+0x2e4> 8ca50000 	lw	a1,0\(a1\)
			2e4: R_MIPS_GOT_OFST	\.data\+0xb4
000002e8 <fn\+0x2e8> 8f850000 	lw	a1,0\(gp\)
			2e8: R_MIPS_GOT_PAGE	\.data\+0xc0
000002ec <fn\+0x2ec> 8ca50000 	lw	a1,0\(a1\)
			2ec: R_MIPS_GOT_OFST	\.data\+0xc0
000002f0 <fn\+0x2f0> 8f850000 	lw	a1,0\(gp\)
			2f0: R_MIPS_GOT_PAGE	\.data\+0xb4
000002f4 <fn\+0x2f4> 00b12821 	addu	a1,a1,s1
000002f8 <fn\+0x2f8> 8ca50000 	lw	a1,0\(a1\)
			2f8: R_MIPS_GOT_OFST	\.data\+0xb4
000002fc <fn\+0x2fc> 8f850000 	lw	a1,0\(gp\)
			2fc: R_MIPS_GOT_PAGE	\.data\+0xc0
00000300 <fn\+0x300> 00b12821 	addu	a1,a1,s1
00000304 <fn\+0x304> 8ca50000 	lw	a1,0\(a1\)
			304: R_MIPS_GOT_OFST	\.data\+0xc0
00000308 <fn\+0x308> 8f810000 	lw	at,0\(gp\)
			308: R_MIPS_GOT_PAGE	\.data\+0xd6
0000030c <fn\+0x30c> 00250821 	addu	at,at,a1
00000310 <fn\+0x310> 8c250000 	lw	a1,0\(at\)
			310: R_MIPS_GOT_OFST	\.data\+0xd6
00000314 <fn\+0x314> 8f810000 	lw	at,0\(gp\)
			314: R_MIPS_GOT_PAGE	\.data\+0xec
00000318 <fn\+0x318> 00250821 	addu	at,at,a1
0000031c <fn\+0x31c> ac250000 	sw	a1,0\(at\)
			31c: R_MIPS_GOT_OFST	\.data\+0xec
00000320 <fn\+0x320> 8f810000 	lw	at,0\(gp\)
			320: R_MIPS_GOT_DISP	\.data\+0xb4
00000324 <fn\+0x324> 8825000[03] 	lwl	a1,[03]\(at\)
00000328 <fn\+0x328> 9825000[03] 	lwr	a1,[03]\(at\)
0000032c <fn\+0x32c> 8f810000 	lw	at,0\(gp\)
			32c: R_MIPS_GOT_DISP	\.data\+0xc0
00000330 <fn\+0x330> 8825000[03] 	lwl	a1,[03]\(at\)
00000334 <fn\+0x334> 9825000[03] 	lwr	a1,[03]\(at\)
00000338 <fn\+0x338> 8f810000 	lw	at,0\(gp\)
			338: R_MIPS_GOT_DISP	\.data\+0xb4
0000033c <fn\+0x33c> 00310821 	addu	at,at,s1
00000340 <fn\+0x340> 8825000[03] 	lwl	a1,[03]\(at\)
00000344 <fn\+0x344> 9825000[03] 	lwr	a1,[03]\(at\)
00000348 <fn\+0x348> 8f810000 	lw	at,0\(gp\)
			348: R_MIPS_GOT_DISP	\.data\+0xc0
0000034c <fn\+0x34c> 00310821 	addu	at,at,s1
00000350 <fn\+0x350> 8825000[03] 	lwl	a1,[03]\(at\)
00000354 <fn\+0x354> 9825000[03] 	lwr	a1,[03]\(at\)
00000358 <fn\+0x358> 8f810000 	lw	at,0\(gp\)
			358: R_MIPS_GOT_DISP	\.data\+0xd6
0000035c <fn\+0x35c> 00250821 	addu	at,at,a1
00000360 <fn\+0x360> 8825000[03] 	lwl	a1,[03]\(at\)
00000364 <fn\+0x364> 9825000[03] 	lwr	a1,[03]\(at\)
00000368 <fn\+0x368> 8f810000 	lw	at,0\(gp\)
			368: R_MIPS_GOT_DISP	\.data\+0xec
0000036c <fn\+0x36c> 00250821 	addu	at,at,a1
00000370 <fn\+0x370> a825000[03] 	swl	a1,[03]\(at\)
00000374 <fn\+0x374> b825000[03] 	swr	a1,[03]\(at\)
00000378 <fn\+0x378> 8f850000 	lw	a1,0\(gp\)
			378: R_MIPS_GOT_DISP	fn2
0000037c <fn\+0x37c> 8f850000 	lw	a1,0\(gp\)
			37c: R_MIPS_GOT_DISP	\.text\+0x404
00000380 <fn\+0x380> 8f990000 	lw	t9,0\(gp\)
			380: R_MIPS_CALL16	fn2
00000384 <fn\+0x384> 8f990000 	lw	t9,0\(gp\)
			384: R_MIPS_GOT_DISP	\.text\+0x404
00000388 <fn\+0x388> 8f990000 	lw	t9,0\(gp\)
			388: R_MIPS_CALL16	fn2
0000038c <fn\+0x38c> 0320f809 	jalr	t9
			38c: R_MIPS_JALR	fn2
00000390 <fn\+0x390> 00000000 	nop
00000394 <fn\+0x394> 8f990000 	lw	t9,0\(gp\)
			394: R_MIPS_GOT_DISP	\.text\+0x404
00000398 <fn\+0x398> 0320f809 	jalr	t9
			398: R_MIPS_JALR	\.text\+0x404
0000039c <fn\+0x39c> 00000000 	nop
000003a0 <fn\+0x3a0> 1000ff17 	b	00000000 <fn>
000003a4 <fn\+0x3a4> 8f850000 	lw	a1,0\(gp\)
			3a4: R_MIPS_GOT_DISP	dg1
000003a8 <fn\+0x3a8> 8f850000 	lw	a1,0\(gp\)
			3a8: R_MIPS_GOT_PAGE	dg2
000003ac <fn\+0x3ac> 10000015 	b	00000404 <fn2>
000003b0 <fn\+0x3b0> 8ca50000 	lw	a1,0\(a1\)
			3b0: R_MIPS_GOT_OFST	dg2
000003b4 <fn\+0x3b4> 1000ff12 	b	00000000 <fn>
000003b8 <fn\+0x3b8> 8f850000 	lw	a1,0\(gp\)
			3b8: R_MIPS_GOT_DISP	\.data\+0x3c
000003bc <fn\+0x3bc> 8f850000 	lw	a1,0\(gp\)
			3bc: R_MIPS_GOT_DISP	\.data\+0xc0
000003c0 <fn\+0x3c0> 10000010 	b	00000404 <fn2>
000003c4 <fn\+0x3c4> 00000000 	nop
000003c8 <fn\+0x3c8> 8f850000 	lw	a1,0\(gp\)
			3c8: R_MIPS_GOT_DISP	\.data\+0x1e27c
000003cc <fn\+0x3cc> 1000ff0c 	b	00000000 <fn>
000003d0 <fn\+0x3d0> 00000000 	nop
000003d4 <fn\+0x3d4> 8f850000 	lw	a1,0\(gp\)
			3d4: R_MIPS_GOT_PAGE	\.data\+0xb4
000003d8 <fn\+0x3d8> 1000000a 	b	00000404 <fn2>
000003dc <fn\+0x3dc> 8ca50000 	lw	a1,0\(a1\)
			3dc: R_MIPS_GOT_OFST	\.data\+0xb4
000003e0 <fn\+0x3e0> 8f850000 	lw	a1,0\(gp\)
			3e0: R_MIPS_GOT_PAGE	\.data\+0x48
000003e4 <fn\+0x3e4> 1000ff06 	b	00000000 <fn>
000003e8 <fn\+0x3e8> 8ca50000 	lw	a1,0\(a1\)
			3e8: R_MIPS_GOT_OFST	\.data\+0x48
000003ec <fn\+0x3ec> 8f810000 	lw	at,0\(gp\)
			3ec: R_MIPS_GOT_PAGE	\.data\+0xd6
000003f0 <fn\+0x3f0> 00250821 	addu	at,at,a1
000003f4 <fn\+0x3f4> 10000003 	b	00000404 <fn2>
000003f8 <fn\+0x3f8> 8c250000 	lw	a1,0\(at\)
			3f8: R_MIPS_GOT_OFST	\.data\+0xd6
	\.\.\.
	\.\.\.
