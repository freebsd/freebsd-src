#as: -m32rx --no-warn-explicit-parallel-conflicts --hidden -O
#objdump: -dr
#name: m32rx

.*: +file format .*

Disassembly of section .text:

0+0000 <bcl>:
   0:	78 00 f0 00 	bcl 0 <bcl> \|\| nop

0+0004 <bncl>:
   4:	79 ff f0 00 	bncl 0 <bcl> \|\| nop

0+0008 <cmpz>:
   8:	00 7d f0 00 	cmpz fp \|\| nop

0+000c <cmpeq>:
   c:	0d 6d f0 00 	cmpeq fp,fp \|\| nop

0+0010 <maclh1>:
  10:	5d cd f0 00 	maclh1 fp,fp \|\| nop

0+0014 <msblo>:
  14:	5d dd f0 00 	msblo fp,fp \|\| nop

0+0018 <mulwu1>:
  18:	5d ad f0 00 	mulwu1 fp,fp \|\| nop

0+001c <macwu1>:
  1c:	5d bd f0 00 	macwu1 fp,fp \|\| nop

0+0020 <sadd>:
  20:	50 e4 f0 00 	sadd \|\| nop

0+0024 <satb>:
  24:	8d 6d 03 00 	satb fp,fp

0+0028 <mulhi>:
  28:	3d 8d f0 00 	mulhi fp,fp,a1 \|\| nop

0+002c <mullo>:
  2c:	3d 1d f0 00 	mullo fp,fp \|\| nop

0+0030 <divh>:
  30:	9d 0d 00 10 	divh fp,fp

0+0034 <machi>:
  34:	3d cd f0 00 	machi fp,fp,a1 \|\| nop

0+0038 <maclo>:
  38:	3d 5d f0 00 	maclo fp,fp \|\| nop

0+003c <mvfachi>:
  3c:	5d f4 f0 00 	mvfachi fp,a1 \|\| nop

0+0040 <mvfacmi>:
  40:	5d f6 f0 00 	mvfacmi fp,a1 \|\| nop

0+0044 <mvfaclo>:
  44:	5d f5 f0 00 	mvfaclo fp,a1 \|\| nop

0+0048 <mvtachi>:
  48:	5d 74 f0 00 	mvtachi fp,a1 \|\| nop

0+004c <mvtaclo>:
  4c:	5d 71 f0 00 	mvtaclo fp \|\| nop

0+0050 <rac>:
  50:	54 90 f0 00 	rac a1 \|\| nop

0+0054 <rac_ds>:
  54:	54 90 f0 00 	rac a1 \|\| nop

0+0058 <rac_dsi>:
  58:	50 94 f0 00 	rac a0,a1 \|\| nop

0+005c <rach>:
  5c:	54 80 f0 00 	rach a1 \|\| nop

0+0060 <rach_ds>:
  60:	50 84 f0 00 	rach a0,a1 \|\| nop

0+0064 <rach_dsi>:
  64:	54 81 f0 00 	rach a1,a0,#0x2 \|\| nop

0+0068 <bc__add>:
  68:	7c 00 8d ad 	bc 68 <bc__add> \|\| add fp,fp
			68: R_M32R_10_PCREL_RELA	bcl
  6c:	7c 00 0d ad 	bc 6c <bc__add\+0x4> -> add fp,fp
			6c: R_M32R_10_PCREL_RELA	bcl

0+0070 <bcl__addi>:
  70:	78 00 cd 4d 	bcl 70 <bcl__addi> \|\| addi fp,#77
			70: R_M32R_10_PCREL_RELA	bcl
  74:	78 00 cd 4d 	bcl 74 <bcl__addi\+0x4> \|\| addi fp,#77
			74: R_M32R_10_PCREL_RELA	bcl

0+0078 <bl__addv>:
  78:	7e 00 8d 8d 	bl 78 <bl__addv> \|\| addv fp,fp
			78: R_M32R_10_PCREL_RELA	bcl
  7c:	7e 00 8d 8d 	bl 7c <bl__addv\+0x4> \|\| addv fp,fp
			7c: R_M32R_10_PCREL_RELA	bcl

0+0080 <bnc__addx>:
  80:	7d 00 8d 9d 	bnc 80 <bnc__addx> \|\| addx fp,fp
			80: R_M32R_10_PCREL_RELA	bcl
  84:	7d 00 0d 9d 	bnc 84 <bnc__addx\+0x4> -> addx fp,fp
			84: R_M32R_10_PCREL_RELA	bcl

0+0088 <bncl__and>:
  88:	79 00 8d cd 	bncl 88 <bncl__and> \|\| and fp,fp
			88: R_M32R_10_PCREL_RELA	bcl
  8c:	79 00 8d cd 	bncl 8c <bncl__and\+0x4> \|\| and fp,fp
			8c: R_M32R_10_PCREL_RELA	bcl

0+0090 <bra__cmp>:
  90:	7f 00 8d 4d 	bra 90 <bra__cmp> \|\| cmp fp,fp
			90: R_M32R_10_PCREL_RELA	bcl
  94:	7f 00 8d 4d 	bra 94 <bra__cmp\+0x4> \|\| cmp fp,fp
			94: R_M32R_10_PCREL_RELA	bcl

0+0098 <jl__cmpeq>:
  98:	1e cd 8d 6d 	jl fp \|\| cmpeq fp,fp
  9c:	1e cd 8d 6d 	jl fp \|\| cmpeq fp,fp

0+00a0 <jmp__cmpu>:
  a0:	1f cd 8d 5d 	jmp fp \|\| cmpu fp,fp
  a4:	1f cd 8d 5d 	jmp fp \|\| cmpu fp,fp

0+00a8 <ld__cmpz>:
  a8:	2d cd 80 71 	ld fp,@fp \|\| cmpz r1
  ac:	2d cd 80 71 	ld fp,@fp \|\| cmpz r1

0+00b0 <ld__ldi>:
  b0:	2d e1 e2 4d 	ld fp,@r1\+ \|\| ldi r2,#77
  b4:	2d e1 e2 4d 	ld fp,@r1\+ \|\| ldi r2,#77

0+00b8 <ldb__mv>:
  b8:	2d 8d 92 8d 	ldb fp,@fp \|\| mv r2,fp
  bc:	2d 8d 12 8d 	ldb fp,@fp -> mv r2,fp

0+00c0 <ldh__neg>:
  c0:	2d ad 82 3d 	ldh fp,@fp \|\| neg r2,fp
  c4:	2d ad 02 3d 	ldh fp,@fp -> neg r2,fp

0+00c8 <ldub__nop>:
  c8:	2d 9d f0 00 	ldub fp,@fp \|\| nop
  cc:	2d 9d f0 00 	ldub fp,@fp \|\| nop

0+00d0 <lduh__not>:
  d0:	2d bd 82 bd 	lduh fp,@fp \|\| not r2,fp
  d4:	2d bd 02 bd 	lduh fp,@fp -> not r2,fp

0+00d8 <lock__or>:
  d8:	2d dd 82 ed 	lock fp,@fp \|\| or r2,fp
  dc:	2d dd 02 ed 	lock fp,@fp -> or r2,fp

0+00e0 <mvfc__sub>:
  e0:	1d 91 82 2d 	mvfc fp,cbr \|\| sub r2,fp
  e4:	1d 91 02 2d 	mvfc fp,cbr -> sub r2,fp

0+00e8 <mvtc__subv>:
  e8:	12 ad 82 0d 	mvtc fp,spi \|\| subv r2,fp
  ec:	12 ad 82 0d 	mvtc fp,spi \|\| subv r2,fp

0+00f0 <rte__subx>:
  f0:	10 d6 82 2d 	rte \|\| sub r2,fp
  f4:	10 d6 02 1d 	rte -> subx r2,fp

0+00f8 <sll__xor>:
  f8:	1d 41 82 dd 	sll fp,r1 \|\| xor r2,fp
  fc:	1d 41 02 dd 	sll fp,r1 -> xor r2,fp

0+0100 <slli__machi>:
 100:	5d 56 b2 4d 	slli fp,#0x16 \|\| machi r2,fp
 104:	5d 56 32 4d 	slli fp,#0x16 -> machi r2,fp

0+0108 <sra__maclh1>:
 108:	1d 2d d2 cd 	sra fp,fp \|\| maclh1 r2,fp
 10c:	1d 2d 52 cd 	sra fp,fp -> maclh1 r2,fp

0+0110 <srai__maclo>:
 110:	5d 36 b2 5d 	srai fp,#0x16 \|\| maclo r2,fp
 114:	5d 36 32 5d 	srai fp,#0x16 -> maclo r2,fp

0+0118 <srl__macwhi>:
 118:	1d 0d b2 6d 	srl fp,fp \|\| macwhi r2,fp
 11c:	1d 0d 32 6d 	srl fp,fp -> macwhi r2,fp

0+0120 <srli__macwlo>:
 120:	5d 16 b2 7d 	srli fp,#0x16 \|\| macwlo r2,fp
 124:	5d 16 32 7d 	srli fp,#0x16 -> macwlo r2,fp

0+0128 <st__macwu1>:
 128:	2d 4d d2 bd 	st fp,@fp \|\| macwu1 r2,fp
 12c:	2d 4d d2 bd 	st fp,@fp \|\| macwu1 r2,fp

0+0130 <st__msblo>:
 130:	2d 6d d2 dd 	st fp,@\+fp \|\| msblo r2,fp
 134:	2d 6d 52 dd 	st fp,@\+fp -> msblo r2,fp

0+0138 <st__mul>:
 138:	2d 7d 92 6d 	st fp,@-fp \|\| mul r2,fp
 13c:	2d 7d 12 6d 	st fp,@-fp -> mul r2,fp

0+0140 <stb__mulhi>:
 140:	2d 0d b2 0d 	stb fp,@fp \|\| mulhi r2,fp
 144:	2d 0d b2 0d 	stb fp,@fp \|\| mulhi r2,fp

0+0148 <sth__mullo>:
 148:	2d 2d b2 1d 	sth fp,@fp \|\| mullo r2,fp
 14c:	2d 2d b2 1d 	sth fp,@fp \|\| mullo r2,fp

0+0150 <trap__mulwhi>:
 150:	10 f2 b2 2d 	trap #0x2 \|\| mulwhi r2,fp
 154:	10 f2 f0 00 	trap #0x2 \|\| nop
 158:	32 2d f0 00 	mulwhi r2,fp \|\| nop

0+015c <unlock__mulwlo>:
 15c:	2d 5d b2 3d 	unlock fp,@fp \|\| mulwlo r2,fp
 160:	2d 5d b2 3d 	unlock fp,@fp \|\| mulwlo r2,fp

0+0164 <add__mulwu1>:
 164:	0d ad d2 ad 	add fp,fp \|\| mulwu1 r2,fp
 168:	0d ad 52 ad 	add fp,fp -> mulwu1 r2,fp

0+016c <addi__mvfachi>:
 16c:	4d 4d d2 f0 	addi fp,#77 \|\| mvfachi r2
 170:	4d 4d d2 f0 	addi fp,#77 \|\| mvfachi r2

0+0174 <addv__mvfaclo>:
 174:	0d 8d d2 f5 	addv fp,fp \|\| mvfaclo r2,a1
 178:	0d 8d d2 f5 	addv fp,fp \|\| mvfaclo r2,a1

0+017c <addx__mvfacmi>:
 17c:	0d 9d d2 f2 	addx fp,fp \|\| mvfacmi r2
 180:	0d 9d d2 f2 	addx fp,fp \|\| mvfacmi r2

0+0184 <and__mvtachi>:
 184:	0d cd d2 70 	and fp,fp \|\| mvtachi r2
 188:	0d cd d2 70 	and fp,fp \|\| mvtachi r2

0+018c <cmp__mvtaclo>:
 18c:	0d 4d d2 71 	cmp fp,fp \|\| mvtaclo r2
 190:	0d 4d d2 71 	cmp fp,fp \|\| mvtaclo r2

0+0194 <cmpeq__rac>:
 194:	0d 6d d4 90 	cmpeq fp,fp \|\| rac a1
 198:	0d 6d d4 90 	cmpeq fp,fp \|\| rac a1

0+019c <cmpu__rach>:
 19c:	0d 5d d0 84 	cmpu fp,fp \|\| rach a0,a1
 1a0:	0d 5d d4 84 	cmpu fp,fp \|\| rach a1,a1

0+01a4 <cmpz__sadd>:
 1a4:	00 7d d0 e4 	cmpz fp \|\| sadd
 1a8:	00 7d d0 e4 	cmpz fp \|\| sadd

0+01ac <sc>:
 1ac:	74 01 d0 e4 	sc \|\| sadd

0+01b0 <snc>:
 1b0:	75 01 d0 e4 	snc \|\| sadd

0+01b4 <jc>:
 1b4:	1c cd f0 00 	jc fp \|\| nop

0+01b8 <jnc>:
 1b8:	1d cd f0 00 	jnc fp \|\| nop

0+01bc <pcmpbz>:
 1bc:	03 7d f0 00 	pcmpbz fp \|\| nop

0+01c0 <sat>:
 1c0:	8d 6d 00 00 	sat fp,fp

0+01c4 <sath>:
 1c4:	8d 6d 02 00 	sath fp,fp

0+01c8 <jc__pcmpbz>:
 1c8:	1c cd 83 7d 	jc fp \|\| pcmpbz fp
 1cc:	1c cd 03 7d 	jc fp -> pcmpbz fp

0+01d0 <jnc__ldi>:
 1d0:	1d cd ed 4d 	jnc fp \|\| ldi fp,#77
 1d4:	1d cd 6d 4d 	jnc fp -> ldi fp,#77

0+01d8 <sc__mv>:
 1d8:	74 01 9d 82 	sc \|\| mv fp,r2
 1dc:	74 01 9d 82 	sc \|\| mv fp,r2

0+01e0 <snc__neg>:
 1e0:	75 01 8d 32 	snc \|\| neg fp,r2
 1e4:	75 01 8d 32 	snc \|\| neg fp,r2

0+01e8 <nop__sadd>:
 1e8:	70 00 d0 e4 	nop \|\| sadd

0+01ec <sadd__nop>:
 1ec:	70 00 d0 e4 	nop \|\| sadd

0+01f0 <sadd__nop_reverse>:
 1f0:	70 00 d0 e4 	nop \|\| sadd

0+01f4 <add__not>:
 1f4:	00 a1 83 b5 	add r0,r1 \|\| not r3,r5

0+01f8 <add__not_dest_clash>:
 1f8:	03 a4 03 b5 	add r3,r4 -> not r3,r5

0+01fc <add__not__src_clash>:
 1fc:	03 a4 05 b3 	add r3,r4 -> not r5,r3

0+0200 <add__not__no_clash>:
 200:	03 a4 84 b5 	add r3,r4 \|\| not r4,r5

0+0204 <mul__sra>:
 204:	13 24 91 62 	sra r3,r4 \|\| mul r1,r2

0+0208 <mul__sra__reverse_src_clash>:
 208:	13 24 91 63 	sra r3,r4 \|\| mul r1,r3

0+020c <bc__add_>:
 20c:	7c 04 01 a2 	bc 21c <label> -> add r1,r2

0+0210 <add__bc>:
 210:	7c 03 83 a4 	bc 21c <label> \|\| add r3,r4

0+0214 <bc__add__forced_parallel>:
 214:	7c 02 85 a6 	bc 21c <label> \|\| add r5,r6

0+0218 <add__bc__forced_parallel>:
 218:	7c 01 87 a8 	bc 21c <label> \|\| add r7,r8

0+021c <label>:
 21c:	70 00 f0 00 	nop \|\| nop

0+0220 <mulwhi>:
 220:	3d 2d 3d ad 	mulwhi fp,fp -> mulwhi fp,fp,a1

0+0224 <mulwlo>:
 224:	3d 3d 3d bd 	mulwlo fp,fp -> mulwlo fp,fp,a1

0+0228 <macwhi>:
 228:	3d 6d 3d ed 	macwhi fp,fp -> macwhi fp,fp,a1

0+022c <macwlo>:
 22c:	3d 7d 3d fd 	macwlo fp,fp -> macwlo fp,fp,a1
