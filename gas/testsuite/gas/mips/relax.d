#as: -KPIC -mips3 -32 -relax-branch
#objdump: -dr --prefix-addresses -mmips:4000
#name: MIPS relax
#stderr: relax.l

# Test relaxation.

.*: +file format .*mips.*

Disassembly of section \.text:
00000000 <foo> lw	at,2\(gp\)
			0: R_MIPS_GOT16	\.text
00000004 <foo\+0x4> addiu	at,at,592
			4: R_MIPS_LO16	\.text
00000008 <foo\+0x8> jr	at
0000000c <foo\+0xc> nop
00000010 <foo\+0x10> lw	at,2\(gp\)
			10: R_MIPS_GOT16	\.text
00000014 <foo\+0x14> addiu	at,at,592
			14: R_MIPS_LO16	\.text
00000018 <foo\+0x18> jalr	at
0000001c <foo\+0x1c> nop
00000020 <foo\+0x20> bne	v0,v1,00000034 <foo\+0x34>
00000024 <foo\+0x24> nop
00000028 <foo\+0x28> lw	at,2\(gp\)
			28: R_MIPS_GOT16	\.text
0000002c <foo\+0x2c> addiu	at,at,592
			2c: R_MIPS_LO16	\.text
00000030 <foo\+0x30> jr	at
00000034 <foo\+0x34> nop
00000038 <foo\+0x38> beq	a0,a1,0000004c <foo\+0x4c>
0000003c <foo\+0x3c> nop
00000040 <foo\+0x40> lw	at,2\(gp\)
			40: R_MIPS_GOT16	\.text
00000044 <foo\+0x44> addiu	at,at,592
			44: R_MIPS_LO16	\.text
00000048 <foo\+0x48> jr	at
0000004c <foo\+0x4c> nop
00000050 <foo\+0x50> bgtz	v0,00000064 <foo\+0x64>
00000054 <foo\+0x54> nop
00000058 <foo\+0x58> lw	at,2\(gp\)
			58: R_MIPS_GOT16	\.text
0000005c <foo\+0x5c> addiu	at,at,592
			5c: R_MIPS_LO16	\.text
00000060 <foo\+0x60> jr	at
00000064 <foo\+0x64> nop
00000068 <foo\+0x68> blez	v1,0000007c <foo\+0x7c>
0000006c <foo\+0x6c> nop
00000070 <foo\+0x70> lw	at,2\(gp\)
			70: R_MIPS_GOT16	\.text
00000074 <foo\+0x74> addiu	at,at,592
			74: R_MIPS_LO16	\.text
00000078 <foo\+0x78> jr	at
0000007c <foo\+0x7c> nop
00000080 <foo\+0x80> bgez	a0,00000094 <foo\+0x94>
00000084 <foo\+0x84> nop
00000088 <foo\+0x88> lw	at,2\(gp\)
			88: R_MIPS_GOT16	\.text
0000008c <foo\+0x8c> addiu	at,at,592
			8c: R_MIPS_LO16	\.text
00000090 <foo\+0x90> jr	at
00000094 <foo\+0x94> nop
00000098 <foo\+0x98> bltz	a1,000000ac <foo\+0xac>
0000009c <foo\+0x9c> nop
000000a0 <foo\+0xa0> lw	at,2\(gp\)
			a0: R_MIPS_GOT16	\.text
000000a4 <foo\+0xa4> addiu	at,at,592
			a4: R_MIPS_LO16	\.text
000000a8 <foo\+0xa8> jr	at
000000ac <foo\+0xac> nop
000000b0 <foo\+0xb0> bc1t	000000c4 <foo\+0xc4>
000000b4 <foo\+0xb4> nop
000000b8 <foo\+0xb8> lw	at,2\(gp\)
			b8: R_MIPS_GOT16	\.text
000000bc <foo\+0xbc> addiu	at,at,592
			bc: R_MIPS_LO16	\.text
000000c0 <foo\+0xc0> jr	at
000000c4 <foo\+0xc4> nop
000000c8 <foo\+0xc8> bc1f	000000dc <foo\+0xdc>
000000cc <foo\+0xcc> nop
000000d0 <foo\+0xd0> lw	at,2\(gp\)
			d0: R_MIPS_GOT16	\.text
000000d4 <foo\+0xd4> addiu	at,at,592
			d4: R_MIPS_LO16	\.text
000000d8 <foo\+0xd8> jr	at
000000dc <foo\+0xdc> nop
000000e0 <foo\+0xe0> bgez	v0,000000f4 <foo\+0xf4>
000000e4 <foo\+0xe4> nop
000000e8 <foo\+0xe8> lw	at,2\(gp\)
			e8: R_MIPS_GOT16	\.text
000000ec <foo\+0xec> addiu	at,at,592
			ec: R_MIPS_LO16	\.text
000000f0 <foo\+0xf0> jalr	at
000000f4 <foo\+0xf4> nop
000000f8 <foo\+0xf8> bltz	v1,0000010c <foo\+0x10c>
000000fc <foo\+0xfc> nop
00000100 <foo\+0x100> lw	at,2\(gp\)
			100: R_MIPS_GOT16	\.text
00000104 <foo\+0x104> addiu	at,at,592
			104: R_MIPS_LO16	\.text
00000108 <foo\+0x108> jalr	at
0000010c <foo\+0x10c> nop
00000110 <foo\+0x110> beql	v0,v1,00000120 <foo\+0x120>
00000114 <foo\+0x114> nop
00000118 <foo\+0x118> beqzl	zero,00000130 <foo\+0x130>
0000011c <foo\+0x11c> nop
00000120 <foo\+0x120> lw	at,2\(gp\)
			120: R_MIPS_GOT16	\.text
00000124 <foo\+0x124> addiu	at,at,592
			124: R_MIPS_LO16	\.text
00000128 <foo\+0x128> jr	at
0000012c <foo\+0x12c> nop
00000130 <foo\+0x130> bnel	a0,a1,00000140 <foo\+0x140>
00000134 <foo\+0x134> nop
00000138 <foo\+0x138> beqzl	zero,00000150 <foo\+0x150>
0000013c <foo\+0x13c> nop
00000140 <foo\+0x140> lw	at,2\(gp\)
			140: R_MIPS_GOT16	\.text
00000144 <foo\+0x144> addiu	at,at,592
			144: R_MIPS_LO16	\.text
00000148 <foo\+0x148> jr	at
0000014c <foo\+0x14c> nop
00000150 <foo\+0x150> blezl	v0,00000160 <foo\+0x160>
00000154 <foo\+0x154> nop
00000158 <foo\+0x158> beqzl	zero,00000170 <foo\+0x170>
0000015c <foo\+0x15c> nop
00000160 <foo\+0x160> lw	at,2\(gp\)
			160: R_MIPS_GOT16	\.text
00000164 <foo\+0x164> addiu	at,at,592
			164: R_MIPS_LO16	\.text
00000168 <foo\+0x168> jr	at
0000016c <foo\+0x16c> nop
00000170 <foo\+0x170> bgtzl	v1,00000180 <foo\+0x180>
00000174 <foo\+0x174> nop
00000178 <foo\+0x178> beqzl	zero,00000190 <foo\+0x190>
0000017c <foo\+0x17c> nop
00000180 <foo\+0x180> lw	at,2\(gp\)
			180: R_MIPS_GOT16	\.text
00000184 <foo\+0x184> addiu	at,at,592
			184: R_MIPS_LO16	\.text
00000188 <foo\+0x188> jr	at
0000018c <foo\+0x18c> nop
00000190 <foo\+0x190> bltzl	a0,000001a0 <foo\+0x1a0>
00000194 <foo\+0x194> nop
00000198 <foo\+0x198> beqzl	zero,000001b0 <foo\+0x1b0>
0000019c <foo\+0x19c> nop
000001a0 <foo\+0x1a0> lw	at,2\(gp\)
			1a0: R_MIPS_GOT16	\.text
000001a4 <foo\+0x1a4> addiu	at,at,592
			1a4: R_MIPS_LO16	\.text
000001a8 <foo\+0x1a8> jr	at
000001ac <foo\+0x1ac> nop
000001b0 <foo\+0x1b0> bgezl	a1,000001c0 <foo\+0x1c0>
000001b4 <foo\+0x1b4> nop
000001b8 <foo\+0x1b8> beqzl	zero,000001d0 <foo\+0x1d0>
000001bc <foo\+0x1bc> nop
000001c0 <foo\+0x1c0> lw	at,2\(gp\)
			1c0: R_MIPS_GOT16	\.text
000001c4 <foo\+0x1c4> addiu	at,at,592
			1c4: R_MIPS_LO16	\.text
000001c8 <foo\+0x1c8> jr	at
000001cc <foo\+0x1cc> nop
000001d0 <foo\+0x1d0> bc1fl	000001e0 <foo\+0x1e0>
000001d4 <foo\+0x1d4> nop
000001d8 <foo\+0x1d8> beqzl	zero,000001f0 <foo\+0x1f0>
000001dc <foo\+0x1dc> nop
000001e0 <foo\+0x1e0> lw	at,2\(gp\)
			1e0: R_MIPS_GOT16	\.text
000001e4 <foo\+0x1e4> addiu	at,at,592
			1e4: R_MIPS_LO16	\.text
000001e8 <foo\+0x1e8> jr	at
000001ec <foo\+0x1ec> nop
000001f0 <foo\+0x1f0> bc1tl	00000200 <foo\+0x200>
000001f4 <foo\+0x1f4> nop
000001f8 <foo\+0x1f8> beqzl	zero,00000210 <foo\+0x210>
000001fc <foo\+0x1fc> nop
00000200 <foo\+0x200> lw	at,2\(gp\)
			200: R_MIPS_GOT16	\.text
00000204 <foo\+0x204> addiu	at,at,592
			204: R_MIPS_LO16	\.text
00000208 <foo\+0x208> jr	at
0000020c <foo\+0x20c> nop
00000210 <foo\+0x210> bltzl	v0,00000220 <foo\+0x220>
00000214 <foo\+0x214> nop
00000218 <foo\+0x218> beqzl	zero,00000230 <foo\+0x230>
0000021c <foo\+0x21c> nop
00000220 <foo\+0x220> lw	at,2\(gp\)
			220: R_MIPS_GOT16	\.text
00000224 <foo\+0x224> addiu	at,at,592
			224: R_MIPS_LO16	\.text
00000228 <foo\+0x228> jalr	at
0000022c <foo\+0x22c> nop
00000230 <foo\+0x230> bgezl	v1,00000240 <foo\+0x240>
00000234 <foo\+0x234> nop
00000238 <foo\+0x238> beqzl	zero,00000250 <foo\+0x250>
0000023c <foo\+0x23c> nop
00000240 <foo\+0x240> lw	at,2\(gp\)
			240: R_MIPS_GOT16	\.text
00000244 <foo\+0x244> addiu	at,at,592
			244: R_MIPS_LO16	\.text
00000248 <foo\+0x248> jalr	at
0000024c <foo\+0x24c> nop
	\.\.\.
00020250 <bar> lw	at,0\(gp\)
			20250: R_MIPS_GOT16	\.text
00020254 <bar\+0x4> addiu	at,at,0
			20254: R_MIPS_LO16	\.text
00020258 <bar\+0x8> jr	at
0002025c <bar\+0xc> nop
00020260 <bar\+0x10> lw	at,0\(gp\)
			20260: R_MIPS_GOT16	\.text
00020264 <bar\+0x14> addiu	at,at,0
			20264: R_MIPS_LO16	\.text
00020268 <bar\+0x18> jalr	at
0002026c <bar\+0x1c> nop
00020270 <bar\+0x20> bne	v0,v1,00020284 <bar\+0x34>
00020274 <bar\+0x24> nop
00020278 <bar\+0x28> lw	at,0\(gp\)
			20278: R_MIPS_GOT16	\.text
0002027c <bar\+0x2c> addiu	at,at,0
			2027c: R_MIPS_LO16	\.text
00020280 <bar\+0x30> jr	at
00020284 <bar\+0x34> nop
00020288 <bar\+0x38> beq	a0,a1,0002029c <bar\+0x4c>
0002028c <bar\+0x3c> nop
00020290 <bar\+0x40> lw	at,0\(gp\)
			20290: R_MIPS_GOT16	\.text
00020294 <bar\+0x44> addiu	at,at,0
			20294: R_MIPS_LO16	\.text
00020298 <bar\+0x48> jr	at
0002029c <bar\+0x4c> nop
000202a0 <bar\+0x50> bgtz	v0,000202b4 <bar\+0x64>
000202a4 <bar\+0x54> nop
000202a8 <bar\+0x58> lw	at,0\(gp\)
			202a8: R_MIPS_GOT16	\.text
000202ac <bar\+0x5c> addiu	at,at,0
			202ac: R_MIPS_LO16	\.text
000202b0 <bar\+0x60> jr	at
000202b4 <bar\+0x64> nop
000202b8 <bar\+0x68> blez	v1,000202cc <bar\+0x7c>
000202bc <bar\+0x6c> nop
000202c0 <bar\+0x70> lw	at,0\(gp\)
			202c0: R_MIPS_GOT16	\.text
000202c4 <bar\+0x74> addiu	at,at,0
			202c4: R_MIPS_LO16	\.text
000202c8 <bar\+0x78> jr	at
000202cc <bar\+0x7c> nop
000202d0 <bar\+0x80> bgez	a0,000202e4 <bar\+0x94>
000202d4 <bar\+0x84> nop
000202d8 <bar\+0x88> lw	at,0\(gp\)
			202d8: R_MIPS_GOT16	\.text
000202dc <bar\+0x8c> addiu	at,at,0
			202dc: R_MIPS_LO16	\.text
000202e0 <bar\+0x90> jr	at
000202e4 <bar\+0x94> nop
000202e8 <bar\+0x98> bltz	a1,000202fc <bar\+0xac>
000202ec <bar\+0x9c> nop
000202f0 <bar\+0xa0> lw	at,0\(gp\)
			202f0: R_MIPS_GOT16	\.text
000202f4 <bar\+0xa4> addiu	at,at,0
			202f4: R_MIPS_LO16	\.text
000202f8 <bar\+0xa8> jr	at
000202fc <bar\+0xac> nop
00020300 <bar\+0xb0> bc1t	00020314 <bar\+0xc4>
00020304 <bar\+0xb4> nop
00020308 <bar\+0xb8> lw	at,0\(gp\)
			20308: R_MIPS_GOT16	\.text
0002030c <bar\+0xbc> addiu	at,at,0
			2030c: R_MIPS_LO16	\.text
00020310 <bar\+0xc0> jr	at
00020314 <bar\+0xc4> nop
00020318 <bar\+0xc8> bc1f	0002032c <bar\+0xdc>
0002031c <bar\+0xcc> nop
00020320 <bar\+0xd0> lw	at,0\(gp\)
			20320: R_MIPS_GOT16	\.text
00020324 <bar\+0xd4> addiu	at,at,0
			20324: R_MIPS_LO16	\.text
00020328 <bar\+0xd8> jr	at
0002032c <bar\+0xdc> nop
00020330 <bar\+0xe0> bgez	v0,00020344 <bar\+0xf4>
00020334 <bar\+0xe4> nop
00020338 <bar\+0xe8> lw	at,0\(gp\)
			20338: R_MIPS_GOT16	\.text
0002033c <bar\+0xec> addiu	at,at,0
			2033c: R_MIPS_LO16	\.text
00020340 <bar\+0xf0> jalr	at
00020344 <bar\+0xf4> nop
00020348 <bar\+0xf8> bltz	v1,0002035c <bar\+0x10c>
0002034c <bar\+0xfc> nop
00020350 <bar\+0x100> lw	at,0\(gp\)
			20350: R_MIPS_GOT16	\.text
00020354 <bar\+0x104> addiu	at,at,0
			20354: R_MIPS_LO16	\.text
00020358 <bar\+0x108> jalr	at
0002035c <bar\+0x10c> nop
00020360 <bar\+0x110> beql	v0,v1,00020370 <bar\+0x120>
00020364 <bar\+0x114> nop
00020368 <bar\+0x118> beqzl	zero,00020380 <bar\+0x130>
0002036c <bar\+0x11c> nop
00020370 <bar\+0x120> lw	at,0\(gp\)
			20370: R_MIPS_GOT16	\.text
00020374 <bar\+0x124> addiu	at,at,0
			20374: R_MIPS_LO16	\.text
00020378 <bar\+0x128> jr	at
0002037c <bar\+0x12c> nop
00020380 <bar\+0x130> bnel	a0,a1,00020390 <bar\+0x140>
00020384 <bar\+0x134> nop
00020388 <bar\+0x138> beqzl	zero,000203a0 <bar\+0x150>
0002038c <bar\+0x13c> nop
00020390 <bar\+0x140> lw	at,0\(gp\)
			20390: R_MIPS_GOT16	\.text
00020394 <bar\+0x144> addiu	at,at,0
			20394: R_MIPS_LO16	\.text
00020398 <bar\+0x148> jr	at
0002039c <bar\+0x14c> nop
000203a0 <bar\+0x150> blezl	v0,000203b0 <bar\+0x160>
000203a4 <bar\+0x154> nop
000203a8 <bar\+0x158> beqzl	zero,000203c0 <bar\+0x170>
000203ac <bar\+0x15c> nop
000203b0 <bar\+0x160> lw	at,0\(gp\)
			203b0: R_MIPS_GOT16	\.text
000203b4 <bar\+0x164> addiu	at,at,0
			203b4: R_MIPS_LO16	\.text
000203b8 <bar\+0x168> jr	at
000203bc <bar\+0x16c> nop
000203c0 <bar\+0x170> bgtzl	v1,000203d0 <bar\+0x180>
000203c4 <bar\+0x174> nop
000203c8 <bar\+0x178> beqzl	zero,000203e0 <bar\+0x190>
000203cc <bar\+0x17c> nop
000203d0 <bar\+0x180> lw	at,0\(gp\)
			203d0: R_MIPS_GOT16	\.text
000203d4 <bar\+0x184> addiu	at,at,0
			203d4: R_MIPS_LO16	\.text
000203d8 <bar\+0x188> jr	at
000203dc <bar\+0x18c> nop
000203e0 <bar\+0x190> bltzl	a0,000203f0 <bar\+0x1a0>
000203e4 <bar\+0x194> nop
000203e8 <bar\+0x198> beqzl	zero,00020400 <bar\+0x1b0>
000203ec <bar\+0x19c> nop
000203f0 <bar\+0x1a0> lw	at,0\(gp\)
			203f0: R_MIPS_GOT16	\.text
000203f4 <bar\+0x1a4> addiu	at,at,0
			203f4: R_MIPS_LO16	\.text
000203f8 <bar\+0x1a8> jr	at
000203fc <bar\+0x1ac> nop
00020400 <bar\+0x1b0> bgezl	a1,00020410 <bar\+0x1c0>
00020404 <bar\+0x1b4> nop
00020408 <bar\+0x1b8> beqzl	zero,00020420 <bar\+0x1d0>
0002040c <bar\+0x1bc> nop
00020410 <bar\+0x1c0> lw	at,0\(gp\)
			20410: R_MIPS_GOT16	\.text
00020414 <bar\+0x1c4> addiu	at,at,0
			20414: R_MIPS_LO16	\.text
00020418 <bar\+0x1c8> jr	at
0002041c <bar\+0x1cc> nop
00020420 <bar\+0x1d0> bc1fl	00020430 <bar\+0x1e0>
00020424 <bar\+0x1d4> nop
00020428 <bar\+0x1d8> beqzl	zero,00020440 <bar\+0x1f0>
0002042c <bar\+0x1dc> nop
00020430 <bar\+0x1e0> lw	at,0\(gp\)
			20430: R_MIPS_GOT16	\.text
00020434 <bar\+0x1e4> addiu	at,at,0
			20434: R_MIPS_LO16	\.text
00020438 <bar\+0x1e8> jr	at
0002043c <bar\+0x1ec> nop
00020440 <bar\+0x1f0> bc1tl	00020450 <bar\+0x200>
00020444 <bar\+0x1f4> nop
00020448 <bar\+0x1f8> beqzl	zero,00020460 <bar\+0x210>
0002044c <bar\+0x1fc> nop
00020450 <bar\+0x200> lw	at,0\(gp\)
			20450: R_MIPS_GOT16	\.text
00020454 <bar\+0x204> addiu	at,at,0
			20454: R_MIPS_LO16	\.text
00020458 <bar\+0x208> jr	at
0002045c <bar\+0x20c> nop
00020460 <bar\+0x210> bltzl	v0,00020470 <bar\+0x220>
00020464 <bar\+0x214> nop
00020468 <bar\+0x218> beqzl	zero,00020480 <bar\+0x230>
0002046c <bar\+0x21c> nop
00020470 <bar\+0x220> lw	at,0\(gp\)
			20470: R_MIPS_GOT16	\.text
00020474 <bar\+0x224> addiu	at,at,0
			20474: R_MIPS_LO16	\.text
00020478 <bar\+0x228> jalr	at
0002047c <bar\+0x22c> nop
00020480 <bar\+0x230> bgezl	v1,00020490 <bar\+0x240>
00020484 <bar\+0x234> nop
00020488 <bar\+0x238> beqzl	zero,000204a0 <bar\+0x250>
0002048c <bar\+0x23c> nop
00020490 <bar\+0x240> lw	at,0\(gp\)
			20490: R_MIPS_GOT16	\.text
00020494 <bar\+0x244> addiu	at,at,0
			20494: R_MIPS_LO16	\.text
00020498 <bar\+0x248> jalr	at
0002049c <bar\+0x24c> nop
