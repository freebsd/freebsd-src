#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS ELF got reloc n64
#as: -64 -KPIC

.*: +file format elf64-.*mips.*

Disassembly of section \.text:
0000000000000000 <fn> df850000 	ld	a1,0\(gp\)
			0: R_MIPS_GOT_DISP	dg1
			0: R_MIPS_NONE	\*ABS\*
			0: R_MIPS_NONE	\*ABS\*
0000000000000004 <fn\+0x4> df850000 	ld	a1,0\(gp\)
			4: R_MIPS_GOT_DISP	dg1
			4: R_MIPS_NONE	\*ABS\*
			4: R_MIPS_NONE	\*ABS\*
0000000000000008 <fn\+0x8> 64a5000c 	daddiu	a1,a1,12
000000000000000c <fn\+0xc> df850000 	ld	a1,0\(gp\)
			c: R_MIPS_GOT_DISP	dg1
			c: R_MIPS_NONE	\*ABS\*
			c: R_MIPS_NONE	\*ABS\*
0000000000000010 <fn\+0x10> 3c010001 	lui	at,0x1
0000000000000014 <fn\+0x14> 3421e240 	ori	at,at,0xe240
0000000000000018 <fn\+0x18> 00a1282d 	daddu	a1,a1,at
000000000000001c <fn\+0x1c> df850000 	ld	a1,0\(gp\)
			1c: R_MIPS_GOT_DISP	dg1
			1c: R_MIPS_NONE	\*ABS\*
			1c: R_MIPS_NONE	\*ABS\*
0000000000000020 <fn\+0x20> 00b1282d 	daddu	a1,a1,s1
0000000000000024 <fn\+0x24> df850000 	ld	a1,0\(gp\)
			24: R_MIPS_GOT_DISP	dg1
			24: R_MIPS_NONE	\*ABS\*
			24: R_MIPS_NONE	\*ABS\*
0000000000000028 <fn\+0x28> 64a5000c 	daddiu	a1,a1,12
000000000000002c <fn\+0x2c> 00b1282d 	daddu	a1,a1,s1
0000000000000030 <fn\+0x30> df850000 	ld	a1,0\(gp\)
			30: R_MIPS_GOT_DISP	dg1
			30: R_MIPS_NONE	\*ABS\*
			30: R_MIPS_NONE	\*ABS\*
0000000000000034 <fn\+0x34> 3c010001 	lui	at,0x1
0000000000000038 <fn\+0x38> 3421e240 	ori	at,at,0xe240
000000000000003c <fn\+0x3c> 00a1282d 	daddu	a1,a1,at
0000000000000040 <fn\+0x40> 00b1282d 	daddu	a1,a1,s1
0000000000000044 <fn\+0x44> df850000 	ld	a1,0\(gp\)
			44: R_MIPS_GOT_PAGE	dg1
			44: R_MIPS_NONE	\*ABS\*
			44: R_MIPS_NONE	\*ABS\*
0000000000000048 <fn\+0x48> dca50000 	ld	a1,0\(a1\)
			48: R_MIPS_GOT_OFST	dg1
			48: R_MIPS_NONE	\*ABS\*
			48: R_MIPS_NONE	\*ABS\*
000000000000004c <fn\+0x4c> df850000 	ld	a1,0\(gp\)
			4c: R_MIPS_GOT_PAGE	dg1\+0xc
			4c: R_MIPS_NONE	\*ABS\*\+0xc
			4c: R_MIPS_NONE	\*ABS\*\+0xc
0000000000000050 <fn\+0x50> dca50000 	ld	a1,0\(a1\)
			50: R_MIPS_GOT_OFST	dg1\+0xc
			50: R_MIPS_NONE	\*ABS\*\+0xc
			50: R_MIPS_NONE	\*ABS\*\+0xc
0000000000000054 <fn\+0x54> df850000 	ld	a1,0\(gp\)
			54: R_MIPS_GOT_PAGE	dg1
			54: R_MIPS_NONE	\*ABS\*
			54: R_MIPS_NONE	\*ABS\*
0000000000000058 <fn\+0x58> 00b1282d 	daddu	a1,a1,s1
000000000000005c <fn\+0x5c> dca50000 	ld	a1,0\(a1\)
			5c: R_MIPS_GOT_OFST	dg1
			5c: R_MIPS_NONE	\*ABS\*
			5c: R_MIPS_NONE	\*ABS\*
0000000000000060 <fn\+0x60> df850000 	ld	a1,0\(gp\)
			60: R_MIPS_GOT_PAGE	dg1\+0xc
			60: R_MIPS_NONE	\*ABS\*\+0xc
			60: R_MIPS_NONE	\*ABS\*\+0xc
0000000000000064 <fn\+0x64> 00b1282d 	daddu	a1,a1,s1
0000000000000068 <fn\+0x68> dca50000 	ld	a1,0\(a1\)
			68: R_MIPS_GOT_OFST	dg1\+0xc
			68: R_MIPS_NONE	\*ABS\*\+0xc
			68: R_MIPS_NONE	\*ABS\*\+0xc
000000000000006c <fn\+0x6c> df810000 	ld	at,0\(gp\)
			6c: R_MIPS_GOT_PAGE	dg1\+0x22
			6c: R_MIPS_NONE	\*ABS\*\+0x22
			6c: R_MIPS_NONE	\*ABS\*\+0x22
0000000000000070 <fn\+0x70> 0025082d 	daddu	at,at,a1
0000000000000074 <fn\+0x74> dc250000 	ld	a1,0\(at\)
			74: R_MIPS_GOT_OFST	dg1\+0x22
			74: R_MIPS_NONE	\*ABS\*\+0x22
			74: R_MIPS_NONE	\*ABS\*\+0x22
0000000000000078 <fn\+0x78> df810000 	ld	at,0\(gp\)
			78: R_MIPS_GOT_PAGE	dg1\+0x38
			78: R_MIPS_NONE	\*ABS\*\+0x38
			78: R_MIPS_NONE	\*ABS\*\+0x38
000000000000007c <fn\+0x7c> 0025082d 	daddu	at,at,a1
0000000000000080 <fn\+0x80> fc250000 	sd	a1,0\(at\)
			80: R_MIPS_GOT_OFST	dg1\+0x38
			80: R_MIPS_NONE	\*ABS\*\+0x38
			80: R_MIPS_NONE	\*ABS\*\+0x38
0000000000000084 <fn\+0x84> df810000 	ld	at,0\(gp\)
			84: R_MIPS_GOT_DISP	dg1
			84: R_MIPS_NONE	\*ABS\*
			84: R_MIPS_NONE	\*ABS\*
0000000000000088 <fn\+0x88> 8825000[03] 	lwl	a1,[03]\(at\)
000000000000008c <fn\+0x8c> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000090 <fn\+0x90> df810000 	ld	at,0\(gp\)
			90: R_MIPS_GOT_DISP	dg1
			90: R_MIPS_NONE	\*ABS\*
			90: R_MIPS_NONE	\*ABS\*
0000000000000094 <fn\+0x94> 6421000c 	daddiu	at,at,12
0000000000000098 <fn\+0x98> 8825000[03] 	lwl	a1,[03]\(at\)
000000000000009c <fn\+0x9c> 9825000[03] 	lwr	a1,[03]\(at\)
00000000000000a0 <fn\+0xa0> df810000 	ld	at,0\(gp\)
			a0: R_MIPS_GOT_DISP	dg1
			a0: R_MIPS_NONE	\*ABS\*
			a0: R_MIPS_NONE	\*ABS\*
00000000000000a4 <fn\+0xa4> 0031082d 	daddu	at,at,s1
00000000000000a8 <fn\+0xa8> 8825000[03] 	lwl	a1,[03]\(at\)
00000000000000ac <fn\+0xac> 9825000[03] 	lwr	a1,[03]\(at\)
00000000000000b0 <fn\+0xb0> df810000 	ld	at,0\(gp\)
			b0: R_MIPS_GOT_DISP	dg1
			b0: R_MIPS_NONE	\*ABS\*
			b0: R_MIPS_NONE	\*ABS\*
00000000000000b4 <fn\+0xb4> 6421000c 	daddiu	at,at,12
00000000000000b8 <fn\+0xb8> 0031082d 	daddu	at,at,s1
00000000000000bc <fn\+0xbc> 8825000[03] 	lwl	a1,[03]\(at\)
00000000000000c0 <fn\+0xc0> 9825000[03] 	lwr	a1,[03]\(at\)
00000000000000c4 <fn\+0xc4> df810000 	ld	at,0\(gp\)
			c4: R_MIPS_GOT_DISP	dg1
			c4: R_MIPS_NONE	\*ABS\*
			c4: R_MIPS_NONE	\*ABS\*
00000000000000c8 <fn\+0xc8> 64210022 	daddiu	at,at,34
00000000000000cc <fn\+0xcc> 0025082d 	daddu	at,at,a1
00000000000000d0 <fn\+0xd0> 8825000[03] 	lwl	a1,[03]\(at\)
00000000000000d4 <fn\+0xd4> 9825000[03] 	lwr	a1,[03]\(at\)
00000000000000d8 <fn\+0xd8> df810000 	ld	at,0\(gp\)
			d8: R_MIPS_GOT_DISP	dg1
			d8: R_MIPS_NONE	\*ABS\*
			d8: R_MIPS_NONE	\*ABS\*
00000000000000dc <fn\+0xdc> 64210038 	daddiu	at,at,56
00000000000000e0 <fn\+0xe0> 0025082d 	daddu	at,at,a1
00000000000000e4 <fn\+0xe4> a825000[03] 	swl	a1,[03]\(at\)
00000000000000e8 <fn\+0xe8> b825000[03] 	swr	a1,[03]\(at\)
00000000000000ec <fn\+0xec> df850000 	ld	a1,0\(gp\)
			ec: R_MIPS_GOT_DISP	\.data\+0x3c
			ec: R_MIPS_NONE	\*ABS\*\+0x3c
			ec: R_MIPS_NONE	\*ABS\*\+0x3c
00000000000000f0 <fn\+0xf0> df850000 	ld	a1,0\(gp\)
			f0: R_MIPS_GOT_DISP	\.data\+0x48
			f0: R_MIPS_NONE	\*ABS\*\+0x48
			f0: R_MIPS_NONE	\*ABS\*\+0x48
00000000000000f4 <fn\+0xf4> df850000 	ld	a1,0\(gp\)
			f4: R_MIPS_GOT_DISP	\.data\+0x1e27c
			f4: R_MIPS_NONE	\*ABS\*\+0x1e27c
			f4: R_MIPS_NONE	\*ABS\*\+0x1e27c
00000000000000f8 <fn\+0xf8> df850000 	ld	a1,0\(gp\)
			f8: R_MIPS_GOT_DISP	\.data\+0x3c
			f8: R_MIPS_NONE	\*ABS\*\+0x3c
			f8: R_MIPS_NONE	\*ABS\*\+0x3c
00000000000000fc <fn\+0xfc> 00b1282d 	daddu	a1,a1,s1
0000000000000100 <fn\+0x100> df850000 	ld	a1,0\(gp\)
			100: R_MIPS_GOT_DISP	\.data\+0x48
			100: R_MIPS_NONE	\*ABS\*\+0x48
			100: R_MIPS_NONE	\*ABS\*\+0x48
0000000000000104 <fn\+0x104> 00b1282d 	daddu	a1,a1,s1
0000000000000108 <fn\+0x108> df850000 	ld	a1,0\(gp\)
			108: R_MIPS_GOT_DISP	\.data\+0x1e27c
			108: R_MIPS_NONE	\*ABS\*\+0x1e27c
			108: R_MIPS_NONE	\*ABS\*\+0x1e27c
000000000000010c <fn\+0x10c> 00b1282d 	daddu	a1,a1,s1
0000000000000110 <fn\+0x110> df850000 	ld	a1,0\(gp\)
			110: R_MIPS_GOT_PAGE	\.data\+0x3c
			110: R_MIPS_NONE	\*ABS\*\+0x3c
			110: R_MIPS_NONE	\*ABS\*\+0x3c
0000000000000114 <fn\+0x114> dca50000 	ld	a1,0\(a1\)
			114: R_MIPS_GOT_OFST	\.data\+0x3c
			114: R_MIPS_NONE	\*ABS\*\+0x3c
			114: R_MIPS_NONE	\*ABS\*\+0x3c
0000000000000118 <fn\+0x118> df850000 	ld	a1,0\(gp\)
			118: R_MIPS_GOT_PAGE	\.data\+0x48
			118: R_MIPS_NONE	\*ABS\*\+0x48
			118: R_MIPS_NONE	\*ABS\*\+0x48
000000000000011c <fn\+0x11c> dca50000 	ld	a1,0\(a1\)
			11c: R_MIPS_GOT_OFST	\.data\+0x48
			11c: R_MIPS_NONE	\*ABS\*\+0x48
			11c: R_MIPS_NONE	\*ABS\*\+0x48
0000000000000120 <fn\+0x120> df850000 	ld	a1,0\(gp\)
			120: R_MIPS_GOT_PAGE	\.data\+0x3c
			120: R_MIPS_NONE	\*ABS\*\+0x3c
			120: R_MIPS_NONE	\*ABS\*\+0x3c
0000000000000124 <fn\+0x124> 00b1282d 	daddu	a1,a1,s1
0000000000000128 <fn\+0x128> dca50000 	ld	a1,0\(a1\)
			128: R_MIPS_GOT_OFST	\.data\+0x3c
			128: R_MIPS_NONE	\*ABS\*\+0x3c
			128: R_MIPS_NONE	\*ABS\*\+0x3c
000000000000012c <fn\+0x12c> df850000 	ld	a1,0\(gp\)
			12c: R_MIPS_GOT_PAGE	\.data\+0x48
			12c: R_MIPS_NONE	\*ABS\*\+0x48
			12c: R_MIPS_NONE	\*ABS\*\+0x48
0000000000000130 <fn\+0x130> 00b1282d 	daddu	a1,a1,s1
0000000000000134 <fn\+0x134> dca50000 	ld	a1,0\(a1\)
			134: R_MIPS_GOT_OFST	\.data\+0x48
			134: R_MIPS_NONE	\*ABS\*\+0x48
			134: R_MIPS_NONE	\*ABS\*\+0x48
0000000000000138 <fn\+0x138> df810000 	ld	at,0\(gp\)
			138: R_MIPS_GOT_PAGE	\.data\+0x5e
			138: R_MIPS_NONE	\*ABS\*\+0x5e
			138: R_MIPS_NONE	\*ABS\*\+0x5e
000000000000013c <fn\+0x13c> 0025082d 	daddu	at,at,a1
0000000000000140 <fn\+0x140> dc250000 	ld	a1,0\(at\)
			140: R_MIPS_GOT_OFST	\.data\+0x5e
			140: R_MIPS_NONE	\*ABS\*\+0x5e
			140: R_MIPS_NONE	\*ABS\*\+0x5e
0000000000000144 <fn\+0x144> df810000 	ld	at,0\(gp\)
			144: R_MIPS_GOT_PAGE	\.data\+0x74
			144: R_MIPS_NONE	\*ABS\*\+0x74
			144: R_MIPS_NONE	\*ABS\*\+0x74
0000000000000148 <fn\+0x148> 0025082d 	daddu	at,at,a1
000000000000014c <fn\+0x14c> fc250000 	sd	a1,0\(at\)
			14c: R_MIPS_GOT_OFST	\.data\+0x74
			14c: R_MIPS_NONE	\*ABS\*\+0x74
			14c: R_MIPS_NONE	\*ABS\*\+0x74
0000000000000150 <fn\+0x150> df810000 	ld	at,0\(gp\)
			150: R_MIPS_GOT_DISP	\.data\+0x3c
			150: R_MIPS_NONE	\*ABS\*\+0x3c
			150: R_MIPS_NONE	\*ABS\*\+0x3c
0000000000000154 <fn\+0x154> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000158 <fn\+0x158> 9825000[03] 	lwr	a1,[03]\(at\)
000000000000015c <fn\+0x15c> df810000 	ld	at,0\(gp\)
			15c: R_MIPS_GOT_DISP	\.data\+0x48
			15c: R_MIPS_NONE	\*ABS\*\+0x48
			15c: R_MIPS_NONE	\*ABS\*\+0x48
0000000000000160 <fn\+0x160> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000164 <fn\+0x164> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000168 <fn\+0x168> df810000 	ld	at,0\(gp\)
			168: R_MIPS_GOT_DISP	\.data\+0x3c
			168: R_MIPS_NONE	\*ABS\*\+0x3c
			168: R_MIPS_NONE	\*ABS\*\+0x3c
000000000000016c <fn\+0x16c> 0031082d 	daddu	at,at,s1
0000000000000170 <fn\+0x170> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000174 <fn\+0x174> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000178 <fn\+0x178> df810000 	ld	at,0\(gp\)
			178: R_MIPS_GOT_DISP	\.data\+0x48
			178: R_MIPS_NONE	\*ABS\*\+0x48
			178: R_MIPS_NONE	\*ABS\*\+0x48
000000000000017c <fn\+0x17c> 0031082d 	daddu	at,at,s1
0000000000000180 <fn\+0x180> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000184 <fn\+0x184> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000188 <fn\+0x188> df810000 	ld	at,0\(gp\)
			188: R_MIPS_GOT_DISP	\.data\+0x5e
			188: R_MIPS_NONE	\*ABS\*\+0x5e
			188: R_MIPS_NONE	\*ABS\*\+0x5e
000000000000018c <fn\+0x18c> 0025082d 	daddu	at,at,a1
0000000000000190 <fn\+0x190> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000194 <fn\+0x194> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000198 <fn\+0x198> df810000 	ld	at,0\(gp\)
			198: R_MIPS_GOT_DISP	\.data\+0x74
			198: R_MIPS_NONE	\*ABS\*\+0x74
			198: R_MIPS_NONE	\*ABS\*\+0x74
000000000000019c <fn\+0x19c> 0025082d 	daddu	at,at,a1
00000000000001a0 <fn\+0x1a0> a825000[03] 	swl	a1,[03]\(at\)
00000000000001a4 <fn\+0x1a4> b825000[03] 	swr	a1,[03]\(at\)
00000000000001a8 <fn\+0x1a8> df850000 	ld	a1,0\(gp\)
			1a8: R_MIPS_GOT_DISP	fn
			1a8: R_MIPS_NONE	\*ABS\*
			1a8: R_MIPS_NONE	\*ABS\*
00000000000001ac <fn\+0x1ac> df850000 	ld	a1,0\(gp\)
			1ac: R_MIPS_GOT_DISP	\.text
			1ac: R_MIPS_NONE	\*ABS\*
			1ac: R_MIPS_NONE	\*ABS\*
00000000000001b0 <fn\+0x1b0> df990000 	ld	t9,0\(gp\)
			1b0: R_MIPS_CALL16	fn
			1b0: R_MIPS_NONE	\*ABS\*
			1b0: R_MIPS_NONE	\*ABS\*
00000000000001b4 <fn\+0x1b4> df990000 	ld	t9,0\(gp\)
			1b4: R_MIPS_GOT_DISP	\.text
			1b4: R_MIPS_NONE	\*ABS\*
			1b4: R_MIPS_NONE	\*ABS\*
00000000000001b8 <fn\+0x1b8> df990000 	ld	t9,0\(gp\)
			1b8: R_MIPS_CALL16	fn
			1b8: R_MIPS_NONE	\*ABS\*
			1b8: R_MIPS_NONE	\*ABS\*
00000000000001bc <fn\+0x1bc> 0320f809 	jalr	t9
			1bc: R_MIPS_JALR	fn
			1bc: R_MIPS_NONE	\*ABS\*
			1bc: R_MIPS_NONE	\*ABS\*
00000000000001c0 <fn\+0x1c0> 00000000 	nop
00000000000001c4 <fn\+0x1c4> df990000 	ld	t9,0\(gp\)
			1c4: R_MIPS_GOT_DISP	\.text
			1c4: R_MIPS_NONE	\*ABS\*
			1c4: R_MIPS_NONE	\*ABS\*
00000000000001c8 <fn\+0x1c8> 0320f809 	jalr	t9
			1c8: R_MIPS_JALR	\.text
			1c8: R_MIPS_NONE	\*ABS\*
			1c8: R_MIPS_NONE	\*ABS\*
00000000000001cc <fn\+0x1cc> 00000000 	nop
00000000000001d0 <fn\+0x1d0> df850000 	ld	a1,0\(gp\)
			1d0: R_MIPS_GOT_DISP	dg2
			1d0: R_MIPS_NONE	\*ABS\*
			1d0: R_MIPS_NONE	\*ABS\*
00000000000001d4 <fn\+0x1d4> df850000 	ld	a1,0\(gp\)
			1d4: R_MIPS_GOT_DISP	dg2
			1d4: R_MIPS_NONE	\*ABS\*
			1d4: R_MIPS_NONE	\*ABS\*
00000000000001d8 <fn\+0x1d8> 64a5000c 	daddiu	a1,a1,12
00000000000001dc <fn\+0x1dc> df850000 	ld	a1,0\(gp\)
			1dc: R_MIPS_GOT_DISP	dg2
			1dc: R_MIPS_NONE	\*ABS\*
			1dc: R_MIPS_NONE	\*ABS\*
00000000000001e0 <fn\+0x1e0> 3c010001 	lui	at,0x1
00000000000001e4 <fn\+0x1e4> 3421e240 	ori	at,at,0xe240
00000000000001e8 <fn\+0x1e8> 00a1282d 	daddu	a1,a1,at
00000000000001ec <fn\+0x1ec> df850000 	ld	a1,0\(gp\)
			1ec: R_MIPS_GOT_DISP	dg2
			1ec: R_MIPS_NONE	\*ABS\*
			1ec: R_MIPS_NONE	\*ABS\*
00000000000001f0 <fn\+0x1f0> 00b1282d 	daddu	a1,a1,s1
00000000000001f4 <fn\+0x1f4> df850000 	ld	a1,0\(gp\)
			1f4: R_MIPS_GOT_DISP	dg2
			1f4: R_MIPS_NONE	\*ABS\*
			1f4: R_MIPS_NONE	\*ABS\*
00000000000001f8 <fn\+0x1f8> 64a5000c 	daddiu	a1,a1,12
00000000000001fc <fn\+0x1fc> 00b1282d 	daddu	a1,a1,s1
0000000000000200 <fn\+0x200> df850000 	ld	a1,0\(gp\)
			200: R_MIPS_GOT_DISP	dg2
			200: R_MIPS_NONE	\*ABS\*
			200: R_MIPS_NONE	\*ABS\*
0000000000000204 <fn\+0x204> 3c010001 	lui	at,0x1
0000000000000208 <fn\+0x208> 3421e240 	ori	at,at,0xe240
000000000000020c <fn\+0x20c> 00a1282d 	daddu	a1,a1,at
0000000000000210 <fn\+0x210> 00b1282d 	daddu	a1,a1,s1
0000000000000214 <fn\+0x214> df850000 	ld	a1,0\(gp\)
			214: R_MIPS_GOT_PAGE	dg2
			214: R_MIPS_NONE	\*ABS\*
			214: R_MIPS_NONE	\*ABS\*
0000000000000218 <fn\+0x218> dca50000 	ld	a1,0\(a1\)
			218: R_MIPS_GOT_OFST	dg2
			218: R_MIPS_NONE	\*ABS\*
			218: R_MIPS_NONE	\*ABS\*
000000000000021c <fn\+0x21c> df850000 	ld	a1,0\(gp\)
			21c: R_MIPS_GOT_PAGE	dg2\+0xc
			21c: R_MIPS_NONE	\*ABS\*\+0xc
			21c: R_MIPS_NONE	\*ABS\*\+0xc
0000000000000220 <fn\+0x220> dca50000 	ld	a1,0\(a1\)
			220: R_MIPS_GOT_OFST	dg2\+0xc
			220: R_MIPS_NONE	\*ABS\*\+0xc
			220: R_MIPS_NONE	\*ABS\*\+0xc
0000000000000224 <fn\+0x224> df850000 	ld	a1,0\(gp\)
			224: R_MIPS_GOT_PAGE	dg2
			224: R_MIPS_NONE	\*ABS\*
			224: R_MIPS_NONE	\*ABS\*
0000000000000228 <fn\+0x228> 00b1282d 	daddu	a1,a1,s1
000000000000022c <fn\+0x22c> dca50000 	ld	a1,0\(a1\)
			22c: R_MIPS_GOT_OFST	dg2
			22c: R_MIPS_NONE	\*ABS\*
			22c: R_MIPS_NONE	\*ABS\*
0000000000000230 <fn\+0x230> df850000 	ld	a1,0\(gp\)
			230: R_MIPS_GOT_PAGE	dg2\+0xc
			230: R_MIPS_NONE	\*ABS\*\+0xc
			230: R_MIPS_NONE	\*ABS\*\+0xc
0000000000000234 <fn\+0x234> 00b1282d 	daddu	a1,a1,s1
0000000000000238 <fn\+0x238> dca50000 	ld	a1,0\(a1\)
			238: R_MIPS_GOT_OFST	dg2\+0xc
			238: R_MIPS_NONE	\*ABS\*\+0xc
			238: R_MIPS_NONE	\*ABS\*\+0xc
000000000000023c <fn\+0x23c> df810000 	ld	at,0\(gp\)
			23c: R_MIPS_GOT_PAGE	dg2\+0x22
			23c: R_MIPS_NONE	\*ABS\*\+0x22
			23c: R_MIPS_NONE	\*ABS\*\+0x22
0000000000000240 <fn\+0x240> 0025082d 	daddu	at,at,a1
0000000000000244 <fn\+0x244> dc250000 	ld	a1,0\(at\)
			244: R_MIPS_GOT_OFST	dg2\+0x22
			244: R_MIPS_NONE	\*ABS\*\+0x22
			244: R_MIPS_NONE	\*ABS\*\+0x22
0000000000000248 <fn\+0x248> df810000 	ld	at,0\(gp\)
			248: R_MIPS_GOT_PAGE	dg2\+0x38
			248: R_MIPS_NONE	\*ABS\*\+0x38
			248: R_MIPS_NONE	\*ABS\*\+0x38
000000000000024c <fn\+0x24c> 0025082d 	daddu	at,at,a1
0000000000000250 <fn\+0x250> fc250000 	sd	a1,0\(at\)
			250: R_MIPS_GOT_OFST	dg2\+0x38
			250: R_MIPS_NONE	\*ABS\*\+0x38
			250: R_MIPS_NONE	\*ABS\*\+0x38
0000000000000254 <fn\+0x254> df810000 	ld	at,0\(gp\)
			254: R_MIPS_GOT_DISP	dg2
			254: R_MIPS_NONE	\*ABS\*
			254: R_MIPS_NONE	\*ABS\*
0000000000000258 <fn\+0x258> 8825000[03] 	lwl	a1,[03]\(at\)
000000000000025c <fn\+0x25c> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000260 <fn\+0x260> df810000 	ld	at,0\(gp\)
			260: R_MIPS_GOT_DISP	dg2
			260: R_MIPS_NONE	\*ABS\*
			260: R_MIPS_NONE	\*ABS\*
0000000000000264 <fn\+0x264> 6421000c 	daddiu	at,at,12
0000000000000268 <fn\+0x268> 8825000[03] 	lwl	a1,[03]\(at\)
000000000000026c <fn\+0x26c> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000270 <fn\+0x270> df810000 	ld	at,0\(gp\)
			270: R_MIPS_GOT_DISP	dg2
			270: R_MIPS_NONE	\*ABS\*
			270: R_MIPS_NONE	\*ABS\*
0000000000000274 <fn\+0x274> 0031082d 	daddu	at,at,s1
0000000000000278 <fn\+0x278> 8825000[03] 	lwl	a1,[03]\(at\)
000000000000027c <fn\+0x27c> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000280 <fn\+0x280> df810000 	ld	at,0\(gp\)
			280: R_MIPS_GOT_DISP	dg2
			280: R_MIPS_NONE	\*ABS\*
			280: R_MIPS_NONE	\*ABS\*
0000000000000284 <fn\+0x284> 6421000c 	daddiu	at,at,12
0000000000000288 <fn\+0x288> 0031082d 	daddu	at,at,s1
000000000000028c <fn\+0x28c> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000290 <fn\+0x290> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000294 <fn\+0x294> df810000 	ld	at,0\(gp\)
			294: R_MIPS_GOT_DISP	dg2
			294: R_MIPS_NONE	\*ABS\*
			294: R_MIPS_NONE	\*ABS\*
0000000000000298 <fn\+0x298> 64210022 	daddiu	at,at,34
000000000000029c <fn\+0x29c> 0025082d 	daddu	at,at,a1
00000000000002a0 <fn\+0x2a0> 8825000[03] 	lwl	a1,[03]\(at\)
00000000000002a4 <fn\+0x2a4> 9825000[03] 	lwr	a1,[03]\(at\)
00000000000002a8 <fn\+0x2a8> df810000 	ld	at,0\(gp\)
			2a8: R_MIPS_GOT_DISP	dg2
			2a8: R_MIPS_NONE	\*ABS\*
			2a8: R_MIPS_NONE	\*ABS\*
00000000000002ac <fn\+0x2ac> 64210038 	daddiu	at,at,56
00000000000002b0 <fn\+0x2b0> 0025082d 	daddu	at,at,a1
00000000000002b4 <fn\+0x2b4> a825000[03] 	swl	a1,[03]\(at\)
00000000000002b8 <fn\+0x2b8> b825000[03] 	swr	a1,[03]\(at\)
00000000000002bc <fn\+0x2bc> df850000 	ld	a1,0\(gp\)
			2bc: R_MIPS_GOT_DISP	\.data\+0xb4
			2bc: R_MIPS_NONE	\*ABS\*\+0xb4
			2bc: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000002c0 <fn\+0x2c0> df850000 	ld	a1,0\(gp\)
			2c0: R_MIPS_GOT_DISP	\.data\+0xc0
			2c0: R_MIPS_NONE	\*ABS\*\+0xc0
			2c0: R_MIPS_NONE	\*ABS\*\+0xc0
00000000000002c4 <fn\+0x2c4> df850000 	ld	a1,0\(gp\)
			2c4: R_MIPS_GOT_DISP	\.data\+0x1e2f4
			2c4: R_MIPS_NONE	\*ABS\*\+0x1e2f4
			2c4: R_MIPS_NONE	\*ABS\*\+0x1e2f4
00000000000002c8 <fn\+0x2c8> df850000 	ld	a1,0\(gp\)
			2c8: R_MIPS_GOT_DISP	\.data\+0xb4
			2c8: R_MIPS_NONE	\*ABS\*\+0xb4
			2c8: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000002cc <fn\+0x2cc> 00b1282d 	daddu	a1,a1,s1
00000000000002d0 <fn\+0x2d0> df850000 	ld	a1,0\(gp\)
			2d0: R_MIPS_GOT_DISP	\.data\+0xc0
			2d0: R_MIPS_NONE	\*ABS\*\+0xc0
			2d0: R_MIPS_NONE	\*ABS\*\+0xc0
00000000000002d4 <fn\+0x2d4> 00b1282d 	daddu	a1,a1,s1
00000000000002d8 <fn\+0x2d8> df850000 	ld	a1,0\(gp\)
			2d8: R_MIPS_GOT_DISP	\.data\+0x1e2f4
			2d8: R_MIPS_NONE	\*ABS\*\+0x1e2f4
			2d8: R_MIPS_NONE	\*ABS\*\+0x1e2f4
00000000000002dc <fn\+0x2dc> 00b1282d 	daddu	a1,a1,s1
00000000000002e0 <fn\+0x2e0> df850000 	ld	a1,0\(gp\)
			2e0: R_MIPS_GOT_PAGE	\.data\+0xb4
			2e0: R_MIPS_NONE	\*ABS\*\+0xb4
			2e0: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000002e4 <fn\+0x2e4> dca50000 	ld	a1,0\(a1\)
			2e4: R_MIPS_GOT_OFST	\.data\+0xb4
			2e4: R_MIPS_NONE	\*ABS\*\+0xb4
			2e4: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000002e8 <fn\+0x2e8> df850000 	ld	a1,0\(gp\)
			2e8: R_MIPS_GOT_PAGE	\.data\+0xc0
			2e8: R_MIPS_NONE	\*ABS\*\+0xc0
			2e8: R_MIPS_NONE	\*ABS\*\+0xc0
00000000000002ec <fn\+0x2ec> dca50000 	ld	a1,0\(a1\)
			2ec: R_MIPS_GOT_OFST	\.data\+0xc0
			2ec: R_MIPS_NONE	\*ABS\*\+0xc0
			2ec: R_MIPS_NONE	\*ABS\*\+0xc0
00000000000002f0 <fn\+0x2f0> df850000 	ld	a1,0\(gp\)
			2f0: R_MIPS_GOT_PAGE	\.data\+0xb4
			2f0: R_MIPS_NONE	\*ABS\*\+0xb4
			2f0: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000002f4 <fn\+0x2f4> 00b1282d 	daddu	a1,a1,s1
00000000000002f8 <fn\+0x2f8> dca50000 	ld	a1,0\(a1\)
			2f8: R_MIPS_GOT_OFST	\.data\+0xb4
			2f8: R_MIPS_NONE	\*ABS\*\+0xb4
			2f8: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000002fc <fn\+0x2fc> df850000 	ld	a1,0\(gp\)
			2fc: R_MIPS_GOT_PAGE	\.data\+0xc0
			2fc: R_MIPS_NONE	\*ABS\*\+0xc0
			2fc: R_MIPS_NONE	\*ABS\*\+0xc0
0000000000000300 <fn\+0x300> 00b1282d 	daddu	a1,a1,s1
0000000000000304 <fn\+0x304> dca50000 	ld	a1,0\(a1\)
			304: R_MIPS_GOT_OFST	\.data\+0xc0
			304: R_MIPS_NONE	\*ABS\*\+0xc0
			304: R_MIPS_NONE	\*ABS\*\+0xc0
0000000000000308 <fn\+0x308> df810000 	ld	at,0\(gp\)
			308: R_MIPS_GOT_PAGE	\.data\+0xd6
			308: R_MIPS_NONE	\*ABS\*\+0xd6
			308: R_MIPS_NONE	\*ABS\*\+0xd6
000000000000030c <fn\+0x30c> 0025082d 	daddu	at,at,a1
0000000000000310 <fn\+0x310> dc250000 	ld	a1,0\(at\)
			310: R_MIPS_GOT_OFST	\.data\+0xd6
			310: R_MIPS_NONE	\*ABS\*\+0xd6
			310: R_MIPS_NONE	\*ABS\*\+0xd6
0000000000000314 <fn\+0x314> df810000 	ld	at,0\(gp\)
			314: R_MIPS_GOT_PAGE	\.data\+0xec
			314: R_MIPS_NONE	\*ABS\*\+0xec
			314: R_MIPS_NONE	\*ABS\*\+0xec
0000000000000318 <fn\+0x318> 0025082d 	daddu	at,at,a1
000000000000031c <fn\+0x31c> fc250000 	sd	a1,0\(at\)
			31c: R_MIPS_GOT_OFST	\.data\+0xec
			31c: R_MIPS_NONE	\*ABS\*\+0xec
			31c: R_MIPS_NONE	\*ABS\*\+0xec
0000000000000320 <fn\+0x320> df810000 	ld	at,0\(gp\)
			320: R_MIPS_GOT_DISP	\.data\+0xb4
			320: R_MIPS_NONE	\*ABS\*\+0xb4
			320: R_MIPS_NONE	\*ABS\*\+0xb4
0000000000000324 <fn\+0x324> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000328 <fn\+0x328> 9825000[03] 	lwr	a1,[03]\(at\)
000000000000032c <fn\+0x32c> df810000 	ld	at,0\(gp\)
			32c: R_MIPS_GOT_DISP	\.data\+0xc0
			32c: R_MIPS_NONE	\*ABS\*\+0xc0
			32c: R_MIPS_NONE	\*ABS\*\+0xc0
0000000000000330 <fn\+0x330> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000334 <fn\+0x334> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000338 <fn\+0x338> df810000 	ld	at,0\(gp\)
			338: R_MIPS_GOT_DISP	\.data\+0xb4
			338: R_MIPS_NONE	\*ABS\*\+0xb4
			338: R_MIPS_NONE	\*ABS\*\+0xb4
000000000000033c <fn\+0x33c> 0031082d 	daddu	at,at,s1
0000000000000340 <fn\+0x340> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000344 <fn\+0x344> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000348 <fn\+0x348> df810000 	ld	at,0\(gp\)
			348: R_MIPS_GOT_DISP	\.data\+0xc0
			348: R_MIPS_NONE	\*ABS\*\+0xc0
			348: R_MIPS_NONE	\*ABS\*\+0xc0
000000000000034c <fn\+0x34c> 0031082d 	daddu	at,at,s1
0000000000000350 <fn\+0x350> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000354 <fn\+0x354> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000358 <fn\+0x358> df810000 	ld	at,0\(gp\)
			358: R_MIPS_GOT_DISP	\.data\+0xd6
			358: R_MIPS_NONE	\*ABS\*\+0xd6
			358: R_MIPS_NONE	\*ABS\*\+0xd6
000000000000035c <fn\+0x35c> 0025082d 	daddu	at,at,a1
0000000000000360 <fn\+0x360> 8825000[03] 	lwl	a1,[03]\(at\)
0000000000000364 <fn\+0x364> 9825000[03] 	lwr	a1,[03]\(at\)
0000000000000368 <fn\+0x368> df810000 	ld	at,0\(gp\)
			368: R_MIPS_GOT_DISP	\.data\+0xec
			368: R_MIPS_NONE	\*ABS\*\+0xec
			368: R_MIPS_NONE	\*ABS\*\+0xec
000000000000036c <fn\+0x36c> 0025082d 	daddu	at,at,a1
0000000000000370 <fn\+0x370> a825000[03] 	swl	a1,[03]\(at\)
0000000000000374 <fn\+0x374> b825000[03] 	swr	a1,[03]\(at\)
0000000000000378 <fn\+0x378> df850000 	ld	a1,0\(gp\)
			378: R_MIPS_GOT_DISP	fn2
			378: R_MIPS_NONE	\*ABS\*
			378: R_MIPS_NONE	\*ABS\*
000000000000037c <fn\+0x37c> df850000 	ld	a1,0\(gp\)
			37c: R_MIPS_GOT_DISP	\.text\+0x404
			37c: R_MIPS_NONE	\*ABS\*\+0x404
			37c: R_MIPS_NONE	\*ABS\*\+0x404
0000000000000380 <fn\+0x380> df990000 	ld	t9,0\(gp\)
			380: R_MIPS_CALL16	fn2
			380: R_MIPS_NONE	\*ABS\*
			380: R_MIPS_NONE	\*ABS\*
0000000000000384 <fn\+0x384> df990000 	ld	t9,0\(gp\)
			384: R_MIPS_GOT_DISP	\.text\+0x404
			384: R_MIPS_NONE	\*ABS\*\+0x404
			384: R_MIPS_NONE	\*ABS\*\+0x404
0000000000000388 <fn\+0x388> df990000 	ld	t9,0\(gp\)
			388: R_MIPS_CALL16	fn2
			388: R_MIPS_NONE	\*ABS\*
			388: R_MIPS_NONE	\*ABS\*
000000000000038c <fn\+0x38c> 0320f809 	jalr	t9
			38c: R_MIPS_JALR	fn2
			38c: R_MIPS_NONE	\*ABS\*
			38c: R_MIPS_NONE	\*ABS\*
0000000000000390 <fn\+0x390> 00000000 	nop
0000000000000394 <fn\+0x394> df990000 	ld	t9,0\(gp\)
			394: R_MIPS_GOT_DISP	\.text\+0x404
			394: R_MIPS_NONE	\*ABS\*\+0x404
			394: R_MIPS_NONE	\*ABS\*\+0x404
0000000000000398 <fn\+0x398> 0320f809 	jalr	t9
			398: R_MIPS_JALR	\.text\+0x404
			398: R_MIPS_NONE	\*ABS\*\+0x404
			398: R_MIPS_NONE	\*ABS\*\+0x404
000000000000039c <fn\+0x39c> 00000000 	nop
00000000000003a0 <fn\+0x3a0> 1000ff17 	b	0000000000000000 <fn>
00000000000003a4 <fn\+0x3a4> df850000 	ld	a1,0\(gp\)
			3a4: R_MIPS_GOT_DISP	dg1
			3a4: R_MIPS_NONE	\*ABS\*
			3a4: R_MIPS_NONE	\*ABS\*
00000000000003a8 <fn\+0x3a8> df850000 	ld	a1,0\(gp\)
			3a8: R_MIPS_GOT_PAGE	dg2
			3a8: R_MIPS_NONE	\*ABS\*
			3a8: R_MIPS_NONE	\*ABS\*
00000000000003ac <fn\+0x3ac> 10000015 	b	0000000000000404 <fn2>
00000000000003b0 <fn\+0x3b0> dca50000 	ld	a1,0\(a1\)
			3b0: R_MIPS_GOT_OFST	dg2
			3b0: R_MIPS_NONE	\*ABS\*
			3b0: R_MIPS_NONE	\*ABS\*
00000000000003b4 <fn\+0x3b4> 1000ff12 	b	0000000000000000 <fn>
00000000000003b8 <fn\+0x3b8> df850000 	ld	a1,0\(gp\)
			3b8: R_MIPS_GOT_DISP	\.data\+0x3c
			3b8: R_MIPS_NONE	\*ABS\*\+0x3c
			3b8: R_MIPS_NONE	\*ABS\*\+0x3c
00000000000003bc <fn\+0x3bc> df850000 	ld	a1,0\(gp\)
			3bc: R_MIPS_GOT_DISP	\.data\+0xc0
			3bc: R_MIPS_NONE	\*ABS\*\+0xc0
			3bc: R_MIPS_NONE	\*ABS\*\+0xc0
00000000000003c0 <fn\+0x3c0> 10000010 	b	0000000000000404 <fn2>
00000000000003c4 <fn\+0x3c4> 00000000 	nop
00000000000003c8 <fn\+0x3c8> df850000 	ld	a1,0\(gp\)
			3c8: R_MIPS_GOT_DISP	\.data\+0x1e27c
			3c8: R_MIPS_NONE	\*ABS\*\+0x1e27c
			3c8: R_MIPS_NONE	\*ABS\*\+0x1e27c
00000000000003cc <fn\+0x3cc> 1000ff0c 	b	0000000000000000 <fn>
00000000000003d0 <fn\+0x3d0> 00000000 	nop
00000000000003d4 <fn\+0x3d4> df850000 	ld	a1,0\(gp\)
			3d4: R_MIPS_GOT_PAGE	\.data\+0xb4
			3d4: R_MIPS_NONE	\*ABS\*\+0xb4
			3d4: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000003d8 <fn\+0x3d8> 1000000a 	b	0000000000000404 <fn2>
00000000000003dc <fn\+0x3dc> dca50000 	ld	a1,0\(a1\)
			3dc: R_MIPS_GOT_OFST	\.data\+0xb4
			3dc: R_MIPS_NONE	\*ABS\*\+0xb4
			3dc: R_MIPS_NONE	\*ABS\*\+0xb4
00000000000003e0 <fn\+0x3e0> df850000 	ld	a1,0\(gp\)
			3e0: R_MIPS_GOT_PAGE	\.data\+0x48
			3e0: R_MIPS_NONE	\*ABS\*\+0x48
			3e0: R_MIPS_NONE	\*ABS\*\+0x48
00000000000003e4 <fn\+0x3e4> 1000ff06 	b	0000000000000000 <fn>
00000000000003e8 <fn\+0x3e8> dca50000 	ld	a1,0\(a1\)
			3e8: R_MIPS_GOT_OFST	\.data\+0x48
			3e8: R_MIPS_NONE	\*ABS\*\+0x48
			3e8: R_MIPS_NONE	\*ABS\*\+0x48
00000000000003ec <fn\+0x3ec> df810000 	ld	at,0\(gp\)
			3ec: R_MIPS_GOT_PAGE	\.data\+0xd6
			3ec: R_MIPS_NONE	\*ABS\*\+0xd6
			3ec: R_MIPS_NONE	\*ABS\*\+0xd6
00000000000003f0 <fn\+0x3f0> 0025082d 	daddu	at,at,a1
00000000000003f4 <fn\+0x3f4> 10000003 	b	0000000000000404 <fn2>
00000000000003f8 <fn\+0x3f8> dc250000 	ld	a1,0\(at\)
			3f8: R_MIPS_GOT_OFST	\.data\+0xd6
			3f8: R_MIPS_NONE	\*ABS\*\+0xd6
			3f8: R_MIPS_NONE	\*ABS\*\+0xd6
	\.\.\.
	\.\.\.
