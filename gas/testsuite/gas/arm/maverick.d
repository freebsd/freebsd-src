#objdump: -dr --prefix-address --show-raw-insn
#name: Maverick
#as: -mcpu=ep9312

# Test the instructions of the Cirrus Maverick floating point co-processor

.*: +file format.*arm.*

Disassembly of section .text:
# load_store:
0*0 <load_store> 0d ?9d ?54 ?ff ? *	cfldrseq	mvf5, ?\[sp, #1020\]
0*4 <load_store\+0x4> 4d ?9b ?e4 ?49 ? *	cfldrsmi	mvf14, ?\[fp, #292\]
0*8 <load_store\+0x8> 7d ?1c ?24 ?ef ? *	cfldrsvc	mvf2, ?\[ip, #-956\]
0*c <load_store\+0xc> bd ?1a ?04 ?ff ? *	cfldrslt	mvf0, ?\[sl, #-1020\]
0*10 <load_store\+0x10> 3d ?11 ?c4 ?27 ? *	cfldrscc	mvf12, ?\[r1, #-156\]
0*14 <load_store\+0x14> ed ?b9 ?d4 ?68 ? *	cfldrs	mvf13, ?\[r9, #416\]!
0*18 <load_store\+0x18> 2d ?30 ?94 ?ff ? *	cfldrscs	mvf9, ?\[r0, #-1020\]!
0*1c <load_store\+0x1c> 9d ?31 ?44 ?27 ? *	cfldrsls	mvf4, ?\[r1, #-156\]!
0*20 <load_store\+0x20> dd ?b9 ?74 ?68 ? *	cfldrsle	mvf7, ?\[r9, #416\]!
0*24 <load_store\+0x24> 6d ?30 ?b4 ?ff ? *	cfldrsvs	mvf11, ?\[r0, #-1020\]!
0*28 <load_store\+0x28> 3c ?31 ?c4 ?27 ? *	cfldrscc	mvf12, ?\[r1\], #-156
0*2c <load_store\+0x2c> ec ?b9 ?d4 ?68 ? *	cfldrs	mvf13, ?\[r9\], #416
0*30 <load_store\+0x30> 2c ?30 ?94 ?ff ? *	cfldrscs	mvf9, ?\[r0\], #-1020
0*34 <load_store\+0x34> 9c ?31 ?44 ?27 ? *	cfldrsls	mvf4, ?\[r1\], #-156
0*38 <load_store\+0x38> dc ?b9 ?74 ?68 ? *	cfldrsle	mvf7, ?\[r9\], #416
0*3c <load_store\+0x3c> 6d ?50 ?b4 ?ff ? *	cfldrdvs	mvd11, ?\[r0, #-1020\]
0*40 <load_store\+0x40> 3d ?51 ?c4 ?27 ? *	cfldrdcc	mvd12, ?\[r1, #-156\]
0*44 <load_store\+0x44> ed ?d9 ?d4 ?68 ? *	cfldrd	mvd13, ?\[r9, #416\]
0*48 <load_store\+0x48> 2d ?50 ?94 ?ff ? *	cfldrdcs	mvd9, ?\[r0, #-1020\]
0*4c <load_store\+0x4c> 9d ?51 ?44 ?27 ? *	cfldrdls	mvd4, ?\[r1, #-156\]
0*50 <load_store\+0x50> dd ?f9 ?74 ?68 ? *	cfldrdle	mvd7, ?\[r9, #416\]!
0*54 <load_store\+0x54> 6d ?70 ?b4 ?ff ? *	cfldrdvs	mvd11, ?\[r0, #-1020\]!
0*58 <load_store\+0x58> 3d ?71 ?c4 ?27 ? *	cfldrdcc	mvd12, ?\[r1, #-156\]!
0*5c <load_store\+0x5c> ed ?f9 ?d4 ?68 ? *	cfldrd	mvd13, ?\[r9, #416\]!
0*60 <load_store\+0x60> 2d ?70 ?94 ?ff ? *	cfldrdcs	mvd9, ?\[r0, #-1020\]!
0*64 <load_store\+0x64> 9c ?71 ?44 ?27 ? *	cfldrdls	mvd4, ?\[r1\], #-156
0*68 <load_store\+0x68> dc ?f9 ?74 ?68 ? *	cfldrdle	mvd7, ?\[r9\], #416
0*6c <load_store\+0x6c> 6c ?70 ?b4 ?ff ? *	cfldrdvs	mvd11, ?\[r0\], #-1020
0*70 <load_store\+0x70> 3c ?71 ?c4 ?27 ? *	cfldrdcc	mvd12, ?\[r1\], #-156
0*74 <load_store\+0x74> ec ?f9 ?d4 ?68 ? *	cfldrd	mvd13, ?\[r9\], #416
0*78 <load_store\+0x78> 2d ?10 ?95 ?ff ? *	cfldr32cs	mvfx9, ?\[r0, #-1020\]
0*7c <load_store\+0x7c> 9d ?11 ?45 ?27 ? *	cfldr32ls	mvfx4, ?\[r1, #-156\]
0*80 <load_store\+0x80> dd ?99 ?75 ?68 ? *	cfldr32le	mvfx7, ?\[r9, #416\]
0*84 <load_store\+0x84> 6d ?10 ?b5 ?ff ? *	cfldr32vs	mvfx11, ?\[r0, #-1020\]
0*88 <load_store\+0x88> 3d ?11 ?c5 ?27 ? *	cfldr32cc	mvfx12, ?\[r1, #-156\]
0*8c <load_store\+0x8c> ed ?b9 ?d5 ?68 ? *	cfldr32	mvfx13, ?\[r9, #416\]!
0*90 <load_store\+0x90> 2d ?30 ?95 ?ff ? *	cfldr32cs	mvfx9, ?\[r0, #-1020\]!
0*94 <load_store\+0x94> 9d ?31 ?45 ?27 ? *	cfldr32ls	mvfx4, ?\[r1, #-156\]!
0*98 <load_store\+0x98> dd ?b9 ?75 ?68 ? *	cfldr32le	mvfx7, ?\[r9, #416\]!
0*9c <load_store\+0x9c> 6d ?30 ?b5 ?ff ? *	cfldr32vs	mvfx11, ?\[r0, #-1020\]!
0*a0 <load_store\+0xa0> 3c ?31 ?c5 ?27 ? *	cfldr32cc	mvfx12, ?\[r1\], #-156
0*a4 <load_store\+0xa4> ec ?b9 ?d5 ?68 ? *	cfldr32	mvfx13, ?\[r9\], #416
0*a8 <load_store\+0xa8> 2c ?30 ?95 ?ff ? *	cfldr32cs	mvfx9, ?\[r0\], #-1020
0*ac <load_store\+0xac> 9c ?31 ?45 ?27 ? *	cfldr32ls	mvfx4, ?\[r1\], #-156
0*b0 <load_store\+0xb0> dc ?b9 ?75 ?68 ? *	cfldr32le	mvfx7, ?\[r9\], #416
0*b4 <load_store\+0xb4> 6d ?50 ?b5 ?ff ? *	cfldr64vs	mvdx11, ?\[r0, #-1020\]
0*b8 <load_store\+0xb8> 3d ?51 ?c5 ?27 ? *	cfldr64cc	mvdx12, ?\[r1, #-156\]
0*bc <load_store\+0xbc> ed ?d9 ?d5 ?68 ? *	cfldr64	mvdx13, ?\[r9, #416\]
0*c0 <load_store\+0xc0> 2d ?50 ?95 ?ff ? *	cfldr64cs	mvdx9, ?\[r0, #-1020\]
0*c4 <load_store\+0xc4> 9d ?51 ?45 ?27 ? *	cfldr64ls	mvdx4, ?\[r1, #-156\]
0*c8 <load_store\+0xc8> dd ?f9 ?75 ?68 ? *	cfldr64le	mvdx7, ?\[r9, #416\]!
0*cc <load_store\+0xcc> 6d ?70 ?b5 ?ff ? *	cfldr64vs	mvdx11, ?\[r0, #-1020\]!
0*d0 <load_store\+0xd0> 3d ?71 ?c5 ?27 ? *	cfldr64cc	mvdx12, ?\[r1, #-156\]!
0*d4 <load_store\+0xd4> ed ?f9 ?d5 ?68 ? *	cfldr64	mvdx13, ?\[r9, #416\]!
0*d8 <load_store\+0xd8> 2d ?70 ?95 ?ff ? *	cfldr64cs	mvdx9, ?\[r0, #-1020\]!
0*dc <load_store\+0xdc> 9c ?71 ?45 ?27 ? *	cfldr64ls	mvdx4, ?\[r1\], #-156
0*e0 <load_store\+0xe0> dc ?f9 ?75 ?68 ? *	cfldr64le	mvdx7, ?\[r9\], #416
0*e4 <load_store\+0xe4> 6c ?70 ?b5 ?ff ? *	cfldr64vs	mvdx11, ?\[r0\], #-1020
0*e8 <load_store\+0xe8> 3c ?71 ?c5 ?27 ? *	cfldr64cc	mvdx12, ?\[r1\], #-156
0*ec <load_store\+0xec> ec ?f9 ?d5 ?68 ? *	cfldr64	mvdx13, ?\[r9\], #416
0*f0 <load_store\+0xf0> 2d ?00 ?94 ?ff ? *	cfstrscs	mvf9, ?\[r0, #-1020\]
0*f4 <load_store\+0xf4> 9d ?01 ?44 ?27 ? *	cfstrsls	mvf4, ?\[r1, #-156\]
0*f8 <load_store\+0xf8> dd ?89 ?74 ?68 ? *	cfstrsle	mvf7, ?\[r9, #416\]
0*fc <load_store\+0xfc> 6d ?00 ?b4 ?ff ? *	cfstrsvs	mvf11, ?\[r0, #-1020\]
0*100 <load_store\+0x100> 3d ?01 ?c4 ?27 ? *	cfstrscc	mvf12, ?\[r1, #-156\]
0*104 <load_store\+0x104> ed ?a9 ?d4 ?68 ? *	cfstrs	mvf13, ?\[r9, #416\]!
0*108 <load_store\+0x108> 2d ?20 ?94 ?ff ? *	cfstrscs	mvf9, ?\[r0, #-1020\]!
0*10c <load_store\+0x10c> 9d ?21 ?44 ?27 ? *	cfstrsls	mvf4, ?\[r1, #-156\]!
0*110 <load_store\+0x110> dd ?a9 ?74 ?68 ? *	cfstrsle	mvf7, ?\[r9, #416\]!
0*114 <load_store\+0x114> 6d ?20 ?b4 ?ff ? *	cfstrsvs	mvf11, ?\[r0, #-1020\]!
0*118 <load_store\+0x118> 3c ?21 ?c4 ?27 ? *	cfstrscc	mvf12, ?\[r1\], #-156
0*11c <load_store\+0x11c> ec ?a9 ?d4 ?68 ? *	cfstrs	mvf13, ?\[r9\], #416
0*120 <load_store\+0x120> 2c ?20 ?94 ?ff ? *	cfstrscs	mvf9, ?\[r0\], #-1020
0*124 <load_store\+0x124> 9c ?21 ?44 ?27 ? *	cfstrsls	mvf4, ?\[r1\], #-156
0*128 <load_store\+0x128> dc ?a9 ?74 ?68 ? *	cfstrsle	mvf7, ?\[r9\], #416
0*12c <load_store\+0x12c> 6d ?40 ?b4 ?ff ? *	cfstrdvs	mvd11, ?\[r0, #-1020\]
0*130 <load_store\+0x130> 3d ?41 ?c4 ?27 ? *	cfstrdcc	mvd12, ?\[r1, #-156\]
0*134 <load_store\+0x134> ed ?c9 ?d4 ?68 ? *	cfstrd	mvd13, ?\[r9, #416\]
0*138 <load_store\+0x138> 2d ?40 ?94 ?ff ? *	cfstrdcs	mvd9, ?\[r0, #-1020\]
0*13c <load_store\+0x13c> 9d ?41 ?44 ?27 ? *	cfstrdls	mvd4, ?\[r1, #-156\]
0*140 <load_store\+0x140> dd ?e9 ?74 ?68 ? *	cfstrdle	mvd7, ?\[r9, #416\]!
0*144 <load_store\+0x144> 6d ?60 ?b4 ?ff ? *	cfstrdvs	mvd11, ?\[r0, #-1020\]!
0*148 <load_store\+0x148> 3d ?61 ?c4 ?27 ? *	cfstrdcc	mvd12, ?\[r1, #-156\]!
0*14c <load_store\+0x14c> ed ?e9 ?d4 ?68 ? *	cfstrd	mvd13, ?\[r9, #416\]!
0*150 <load_store\+0x150> 2d ?60 ?94 ?ff ? *	cfstrdcs	mvd9, ?\[r0, #-1020\]!
0*154 <load_store\+0x154> 9c ?61 ?44 ?27 ? *	cfstrdls	mvd4, ?\[r1\], #-156
0*158 <load_store\+0x158> dc ?e9 ?74 ?68 ? *	cfstrdle	mvd7, ?\[r9\], #416
0*15c <load_store\+0x15c> 6c ?60 ?b4 ?ff ? *	cfstrdvs	mvd11, ?\[r0\], #-1020
0*160 <load_store\+0x160> 3c ?61 ?c4 ?27 ? *	cfstrdcc	mvd12, ?\[r1\], #-156
0*164 <load_store\+0x164> ec ?e9 ?d4 ?68 ? *	cfstrd	mvd13, ?\[r9\], #416
0*168 <load_store\+0x168> 2d ?00 ?95 ?ff ? *	cfstr32cs	mvfx9, ?\[r0, #-1020\]
0*16c <load_store\+0x16c> 9d ?01 ?45 ?27 ? *	cfstr32ls	mvfx4, ?\[r1, #-156\]
0*170 <load_store\+0x170> dd ?89 ?75 ?68 ? *	cfstr32le	mvfx7, ?\[r9, #416\]
0*174 <load_store\+0x174> 6d ?00 ?b5 ?ff ? *	cfstr32vs	mvfx11, ?\[r0, #-1020\]
0*178 <load_store\+0x178> 3d ?01 ?c5 ?27 ? *	cfstr32cc	mvfx12, ?\[r1, #-156\]
0*17c <load_store\+0x17c> ed ?a9 ?d5 ?68 ? *	cfstr32	mvfx13, ?\[r9, #416\]!
0*180 <load_store\+0x180> 2d ?20 ?95 ?ff ? *	cfstr32cs	mvfx9, ?\[r0, #-1020\]!
0*184 <load_store\+0x184> 9d ?21 ?45 ?27 ? *	cfstr32ls	mvfx4, ?\[r1, #-156\]!
0*188 <load_store\+0x188> dd ?a9 ?75 ?68 ? *	cfstr32le	mvfx7, ?\[r9, #416\]!
0*18c <load_store\+0x18c> 6d ?20 ?b5 ?ff ? *	cfstr32vs	mvfx11, ?\[r0, #-1020\]!
0*190 <load_store\+0x190> 3c ?21 ?c5 ?27 ? *	cfstr32cc	mvfx12, ?\[r1\], #-156
0*194 <load_store\+0x194> ec ?a9 ?d5 ?68 ? *	cfstr32	mvfx13, ?\[r9\], #416
0*198 <load_store\+0x198> 2c ?20 ?95 ?ff ? *	cfstr32cs	mvfx9, ?\[r0\], #-1020
0*19c <load_store\+0x19c> 9c ?21 ?45 ?27 ? *	cfstr32ls	mvfx4, ?\[r1\], #-156
0*1a0 <load_store\+0x1a0> dc ?a9 ?75 ?68 ? *	cfstr32le	mvfx7, ?\[r9\], #416
0*1a4 <load_store\+0x1a4> 6d ?40 ?b5 ?ff ? *	cfstr64vs	mvdx11, ?\[r0, #-1020\]
0*1a8 <load_store\+0x1a8> 3d ?41 ?c5 ?27 ? *	cfstr64cc	mvdx12, ?\[r1, #-156\]
0*1ac <load_store\+0x1ac> ed ?c9 ?d5 ?68 ? *	cfstr64	mvdx13, ?\[r9, #416\]
0*1b0 <load_store\+0x1b0> 2d ?40 ?95 ?ff ? *	cfstr64cs	mvdx9, ?\[r0, #-1020\]
0*1b4 <load_store\+0x1b4> 9d ?41 ?45 ?27 ? *	cfstr64ls	mvdx4, ?\[r1, #-156\]
0*1b8 <load_store\+0x1b8> dd ?e9 ?75 ?68 ? *	cfstr64le	mvdx7, ?\[r9, #416\]!
0*1bc <load_store\+0x1bc> 6d ?60 ?b5 ?ff ? *	cfstr64vs	mvdx11, ?\[r0, #-1020\]!
0*1c0 <load_store\+0x1c0> 3d ?61 ?c5 ?27 ? *	cfstr64cc	mvdx12, ?\[r1, #-156\]!
0*1c4 <load_store\+0x1c4> ed ?e9 ?d5 ?68 ? *	cfstr64	mvdx13, ?\[r9, #416\]!
0*1c8 <load_store\+0x1c8> 2d ?60 ?95 ?ff ? *	cfstr64cs	mvdx9, ?\[r0, #-1020\]!
0*1cc <load_store\+0x1cc> 9c ?61 ?45 ?27 ? *	cfstr64ls	mvdx4, ?\[r1\], #-156
0*1d0 <load_store\+0x1d0> dc ?e9 ?75 ?68 ? *	cfstr64le	mvdx7, ?\[r9\], #416
0*1d4 <load_store\+0x1d4> 6c ?60 ?b5 ?ff ? *	cfstr64vs	mvdx11, ?\[r0\], #-1020
0*1d8 <load_store\+0x1d8> 3c ?61 ?c5 ?27 ? *	cfstr64cc	mvdx12, ?\[r1\], #-156
0*1dc <load_store\+0x1dc> ec ?e9 ?d5 ?68 ? *	cfstr64	mvdx13, ?\[r9\], #416
# move:
0*1e0 <move> 2e ?09 ?04 ?50 ? *	cfmvsrcs	mvf9, ?r0
0*1e4 <move\+0x4> 5e ?0f ?74 ?50 ? *	cfmvsrpl	mvf15, ?r7
0*1e8 <move\+0x8> 9e ?04 ?14 ?50 ? *	cfmvsrls	mvf4, ?r1
0*1ec <move\+0xc> 3e ?08 ?24 ?50 ? *	cfmvsrcc	mvf8, ?r2
0*1f0 <move\+0x10> 7e ?02 ?c4 ?50 ? *	cfmvsrvc	mvf2, ?ip
0*1f4 <move\+0x14> ce ?1b ?94 ?50 ? *	cfmvrsgt	r9, ?mvf11
0*1f8 <move\+0x18> 0e ?15 ?a4 ?50 ? *	cfmvrseq	sl, ?mvf5
0*1fc <move\+0x1c> ee ?1c ?44 ?50 ? *	cfmvrs	r4, ?mvf12
0*200 <move\+0x20> ae ?18 ?b4 ?50 ? *	cfmvrsge	fp, ?mvf8
0*204 <move\+0x24> ee ?16 ?54 ?50 ? *	cfmvrs	r5, ?mvf6
0*208 <move\+0x28> be ?04 ?94 ?10 ? *	cfmvdlrlt	mvd4, ?r9
0*20c <move\+0x2c> 9e ?00 ?a4 ?10 ? *	cfmvdlrls	mvd0, ?sl
0*210 <move\+0x30> ee ?0a ?44 ?10 ? *	cfmvdlr	mvd10, ?r4
0*214 <move\+0x34> 4e ?0e ?b4 ?10 ? *	cfmvdlrmi	mvd14, ?fp
0*218 <move\+0x38> 8e ?0d ?54 ?10 ? *	cfmvdlrhi	mvd13, ?r5
0*21c <move\+0x3c> 2e ?1c ?c4 ?10 ? *	cfmvrdlcs	ip, ?mvd12
0*220 <move\+0x40> 6e ?10 ?34 ?10 ? *	cfmvrdlvs	r3, ?mvd0
0*224 <move\+0x44> 7e ?1e ?d4 ?10 ? *	cfmvrdlvc	sp, ?mvd14
0*228 <move\+0x48> 3e ?1a ?e4 ?10 ? *	cfmvrdlcc	lr, ?mvd10
0*22c <move\+0x4c> 1e ?1f ?84 ?10 ? *	cfmvrdlne	r8, ?mvd15
0*230 <move\+0x50> de ?06 ?c4 ?30 ? *	cfmvdhrle	mvd6, ?ip
0*234 <move\+0x54> 4e ?02 ?34 ?30 ? *	cfmvdhrmi	mvd2, ?r3
0*238 <move\+0x58> 0e ?05 ?d4 ?30 ? *	cfmvdhreq	mvd5, ?sp
0*23c <move\+0x5c> ae ?09 ?e4 ?30 ? *	cfmvdhrge	mvd9, ?lr
0*240 <move\+0x60> ee ?03 ?84 ?30 ? *	cfmvdhr	mvd3, ?r8
0*244 <move\+0x64> de ?12 ?54 ?30 ? *	cfmvrdhle	r5, ?mvd2
0*248 <move\+0x68> 1e ?16 ?64 ?30 ? *	cfmvrdhne	r6, ?mvd6
0*24c <move\+0x6c> be ?17 ?04 ?30 ? *	cfmvrdhlt	r0, ?mvd7
0*250 <move\+0x70> 5e ?13 ?74 ?30 ? *	cfmvrdhpl	r7, ?mvd3
0*254 <move\+0x74> ce ?11 ?14 ?30 ? *	cfmvrdhgt	r1, ?mvd1
0*258 <move\+0x78> 8e ?0f ?55 ?10 ? *	cfmv64lrhi	mvdx15, ?r5
0*25c <move\+0x7c> 6e ?0b ?65 ?10 ? *	cfmv64lrvs	mvdx11, ?r6
0*260 <move\+0x80> 2e ?09 ?05 ?10 ? *	cfmv64lrcs	mvdx9, ?r0
0*264 <move\+0x84> 5e ?0f ?75 ?10 ? *	cfmv64lrpl	mvdx15, ?r7
0*268 <move\+0x88> 9e ?04 ?15 ?10 ? *	cfmv64lrls	mvdx4, ?r1
0*26c <move\+0x8c> 3e ?1d ?85 ?10 ? *	cfmvr64lcc	r8, ?mvdx13
0*270 <move\+0x90> 7e ?11 ?f5 ?10 ? *	cfmvr64lvc	pc, ?mvdx1
0*274 <move\+0x94> ce ?1b ?95 ?10 ? *	cfmvr64lgt	r9, ?mvdx11
0*278 <move\+0x98> 0e ?15 ?a5 ?10 ? *	cfmvr64leq	sl, ?mvdx5
0*27c <move\+0x9c> ee ?1c ?45 ?10 ? *	cfmvr64l	r4, ?mvdx12
0*280 <move\+0xa0> ae ?01 ?85 ?30 ? *	cfmv64hrge	mvdx1, ?r8
0*284 <move\+0xa4> ee ?0d ?f5 ?30 ? *	cfmv64hr	mvdx13, ?pc
0*288 <move\+0xa8> be ?04 ?95 ?30 ? *	cfmv64hrlt	mvdx4, ?r9
0*28c <move\+0xac> 9e ?00 ?a5 ?30 ? *	cfmv64hrls	mvdx0, ?sl
0*290 <move\+0xb0> ee ?0a ?45 ?30 ? *	cfmv64hr	mvdx10, ?r4
0*294 <move\+0xb4> 4e ?13 ?15 ?30 ? *	cfmvr64hmi	r1, ?mvdx3
0*298 <move\+0xb8> 8e ?17 ?25 ?30 ? *	cfmvr64hhi	r2, ?mvdx7
0*29c <move\+0xbc> 2e ?1c ?c5 ?30 ? *	cfmvr64hcs	ip, ?mvdx12
0*2a0 <move\+0xc0> 6e ?10 ?35 ?30 ? *	cfmvr64hvs	r3, ?mvdx0
0*2a4 <move\+0xc4> 7e ?1e ?d5 ?30 ? *	cfmvr64hvc	sp, ?mvdx14
0*2a8 <move\+0xc8> 3e ?2a ?04 ?40 ? *	cfmval32cc	mvax0, ?mvfx10
0*2ac <move\+0xcc> 1e ?2f ?14 ?40 ? *	cfmval32ne	mvax1, ?mvfx15
0*2b0 <move\+0xd0> de ?2b ?04 ?40 ? *	cfmval32le	mvax0, ?mvfx11
0*2b4 <move\+0xd4> 4e ?29 ?04 ?40 ? *	cfmval32mi	mvax0, ?mvfx9
0*2b8 <move\+0xd8> 0e ?2f ?14 ?40 ? *	cfmval32eq	mvax1, ?mvfx15
0*2bc <move\+0xdc> ae ?10 ?94 ?40 ? *	cfmv32alge	mvfx9, ?mvax0
0*2c0 <move\+0xe0> ee ?11 ?34 ?40 ? *	cfmv32al	mvfx3, ?mvax1
0*2c4 <move\+0xe4> de ?10 ?74 ?40 ? *	cfmv32alle	mvfx7, ?mvax0
0*2c8 <move\+0xe8> 1e ?10 ?c4 ?40 ? *	cfmv32alne	mvfx12, ?mvax0
0*2cc <move\+0xec> be ?11 ?04 ?40 ? *	cfmv32allt	mvfx0, ?mvax1
0*2d0 <move\+0xf0> 5e ?23 ?24 ?60 ? *	cfmvam32pl	mvax2, ?mvfx3
0*2d4 <move\+0xf4> ce ?21 ?14 ?60 ? *	cfmvam32gt	mvax1, ?mvfx1
0*2d8 <move\+0xf8> 8e ?2d ?34 ?60 ? *	cfmvam32hi	mvax3, ?mvfx13
0*2dc <move\+0xfc> 6e ?24 ?34 ?60 ? *	cfmvam32vs	mvax3, ?mvfx4
0*2e0 <move\+0x100> 2e ?20 ?14 ?60 ? *	cfmvam32cs	mvax1, ?mvfx0
0*2e4 <move\+0x104> 5e ?12 ?f4 ?60 ? *	cfmv32ampl	mvfx15, ?mvax2
0*2e8 <move\+0x108> 9e ?11 ?44 ?60 ? *	cfmv32amls	mvfx4, ?mvax1
0*2ec <move\+0x10c> 3e ?13 ?84 ?60 ? *	cfmv32amcc	mvfx8, ?mvax3
0*2f0 <move\+0x110> 7e ?13 ?24 ?60 ? *	cfmv32amvc	mvfx2, ?mvax3
0*2f4 <move\+0x114> ce ?11 ?64 ?60 ? *	cfmv32amgt	mvfx6, ?mvax1
0*2f8 <move\+0x118> 0e ?25 ?14 ?80 ? *	cfmvah32eq	mvax1, ?mvfx5
0*2fc <move\+0x11c> ee ?2c ?24 ?80 ? *	cfmvah32	mvax2, ?mvfx12
0*300 <move\+0x120> ae ?28 ?34 ?80 ? *	cfmvah32ge	mvax3, ?mvfx8
0*304 <move\+0x124> ee ?26 ?24 ?80 ? *	cfmvah32	mvax2, ?mvfx6
0*308 <move\+0x128> be ?22 ?24 ?80 ? *	cfmvah32lt	mvax2, ?mvfx2
0*30c <move\+0x12c> 9e ?11 ?04 ?80 ? *	cfmv32ahls	mvfx0, ?mvax1
0*310 <move\+0x130> ee ?12 ?a4 ?80 ? *	cfmv32ah	mvfx10, ?mvax2
0*314 <move\+0x134> 4e ?13 ?e4 ?80 ? *	cfmv32ahmi	mvfx14, ?mvax3
0*318 <move\+0x138> 8e ?12 ?d4 ?80 ? *	cfmv32ahhi	mvfx13, ?mvax2
0*31c <move\+0x13c> 2e ?12 ?14 ?80 ? *	cfmv32ahcs	mvfx1, ?mvax2
0*320 <move\+0x140> 6e ?20 ?14 ?a0 ? *	cfmva32vs	mvax1, ?mvfx0
0*324 <move\+0x144> 7e ?2e ?34 ?a0 ? *	cfmva32vc	mvax3, ?mvfx14
0*328 <move\+0x148> 3e ?2a ?04 ?a0 ? *	cfmva32cc	mvax0, ?mvfx10
0*32c <move\+0x14c> 1e ?2f ?14 ?a0 ? *	cfmva32ne	mvax1, ?mvfx15
0*330 <move\+0x150> de ?2b ?04 ?a0 ? *	cfmva32le	mvax0, ?mvfx11
0*334 <move\+0x154> 4e ?11 ?24 ?a0 ? *	cfmv32ami	mvfx2, ?mvax1
0*338 <move\+0x158> 0e ?13 ?54 ?a0 ? *	cfmv32aeq	mvfx5, ?mvax3
0*33c <move\+0x15c> ae ?10 ?94 ?a0 ? *	cfmv32age	mvfx9, ?mvax0
0*340 <move\+0x160> ee ?11 ?34 ?a0 ? *	cfmv32a	mvfx3, ?mvax1
0*344 <move\+0x164> de ?10 ?74 ?a0 ? *	cfmv32ale	mvfx7, ?mvax0
0*348 <move\+0x168> 1e ?26 ?24 ?c0 ? *	cfmva64ne	mvax2, ?mvdx6
0*34c <move\+0x16c> be ?27 ?04 ?c0 ? *	cfmva64lt	mvax0, ?mvdx7
0*350 <move\+0x170> 5e ?23 ?24 ?c0 ? *	cfmva64pl	mvax2, ?mvdx3
0*354 <move\+0x174> ce ?21 ?14 ?c0 ? *	cfmva64gt	mvax1, ?mvdx1
0*358 <move\+0x178> 8e ?2d ?34 ?c0 ? *	cfmva64hi	mvax3, ?mvdx13
0*35c <move\+0x17c> 6e ?12 ?b4 ?c0 ? *	cfmv64avs	mvdx11, ?mvax2
0*360 <move\+0x180> 2e ?10 ?94 ?c0 ? *	cfmv64acs	mvdx9, ?mvax0
0*364 <move\+0x184> 5e ?12 ?f4 ?c0 ? *	cfmv64apl	mvdx15, ?mvax2
0*368 <move\+0x188> 9e ?11 ?44 ?c0 ? *	cfmv64als	mvdx4, ?mvax1
0*36c <move\+0x18c> 3e ?13 ?84 ?c0 ? *	cfmv64acc	mvdx8, ?mvax3
0*370 <move\+0x190> 7e ?20 ?14 ?e0 ? *	cfmvsc32vc	dspsc, ?mvdx1
0*374 <move\+0x194> ce ?20 ?b4 ?e0 ? *	cfmvsc32gt	dspsc, ?mvdx11
0*378 <move\+0x198> 0e ?20 ?54 ?e0 ? *	cfmvsc32eq	dspsc, ?mvdx5
0*37c <move\+0x19c> ee ?20 ?c4 ?e0 ? *	cfmvsc32	dspsc, ?mvdx12
0*380 <move\+0x1a0> ae ?20 ?84 ?e0 ? *	cfmvsc32ge	dspsc, ?mvdx8
0*384 <move\+0x1a4> ee ?10 ?d4 ?e0 ? *	cfmv32sc	mvdx13, ?dspsc
0*388 <move\+0x1a8> be ?10 ?44 ?e0 ? *	cfmv32sclt	mvdx4, ?dspsc
0*38c <move\+0x1ac> 9e ?10 ?04 ?e0 ? *	cfmv32scls	mvdx0, ?dspsc
0*390 <move\+0x1b0> ee ?10 ?a4 ?e0 ? *	cfmv32sc	mvdx10, ?dspsc
0*394 <move\+0x1b4> 4e ?10 ?e4 ?e0 ? *	cfmv32scmi	mvdx14, ?dspsc
0*398 <move\+0x1b8> 8e ?07 ?d4 ?00 ? *	cfcpyshi	mvf13, ?mvf7
0*39c <move\+0x1bc> 2e ?0c ?14 ?00 ? *	cfcpyscs	mvf1, ?mvf12
0*3a0 <move\+0x1c0> 6e ?00 ?b4 ?00 ? *	cfcpysvs	mvf11, ?mvf0
0*3a4 <move\+0x1c4> 7e ?0e ?54 ?00 ? *	cfcpysvc	mvf5, ?mvf14
0*3a8 <move\+0x1c8> 3e ?0a ?c4 ?00 ? *	cfcpyscc	mvf12, ?mvf10
0*3ac <move\+0x1cc> 1e ?0f ?84 ?20 ? *	cfcpydne	mvd8, ?mvd15
0*3b0 <move\+0x1d0> de ?0b ?64 ?20 ? *	cfcpydle	mvd6, ?mvd11
0*3b4 <move\+0x1d4> 4e ?09 ?24 ?20 ? *	cfcpydmi	mvd2, ?mvd9
0*3b8 <move\+0x1d8> 0e ?0f ?54 ?20 ? *	cfcpydeq	mvd5, ?mvd15
0*3bc <move\+0x1dc> ae ?04 ?94 ?20 ? *	cfcpydge	mvd9, ?mvd4
# conv:
0*3c0 <conv> ee ?08 ?34 ?60 ? *	cfcvtsd	mvd3, ?mvf8
0*3c4 <conv\+0x4> de ?02 ?74 ?60 ? *	cfcvtsdle	mvd7, ?mvf2
0*3c8 <conv\+0x8> 1e ?06 ?c4 ?60 ? *	cfcvtsdne	mvd12, ?mvf6
0*3cc <conv\+0xc> be ?07 ?04 ?60 ? *	cfcvtsdlt	mvd0, ?mvf7
0*3d0 <conv\+0x10> 5e ?03 ?e4 ?60 ? *	cfcvtsdpl	mvd14, ?mvf3
0*3d4 <conv\+0x14> ce ?01 ?a4 ?40 ? *	cfcvtdsgt	mvf10, ?mvd1
0*3d8 <conv\+0x18> 8e ?0d ?f4 ?40 ? *	cfcvtdshi	mvf15, ?mvd13
0*3dc <conv\+0x1c> 6e ?04 ?b4 ?40 ? *	cfcvtdsvs	mvf11, ?mvd4
0*3e0 <conv\+0x20> 2e ?00 ?94 ?40 ? *	cfcvtdscs	mvf9, ?mvd0
0*3e4 <conv\+0x24> 5e ?0a ?f4 ?40 ? *	cfcvtdspl	mvf15, ?mvd10
0*3e8 <conv\+0x28> 9e ?0e ?44 ?80 ? *	cfcvt32sls	mvf4, ?mvfx14
0*3ec <conv\+0x2c> 3e ?0d ?84 ?80 ? *	cfcvt32scc	mvf8, ?mvfx13
0*3f0 <conv\+0x30> 7e ?01 ?24 ?80 ? *	cfcvt32svc	mvf2, ?mvfx1
0*3f4 <conv\+0x34> ce ?0b ?64 ?80 ? *	cfcvt32sgt	mvf6, ?mvfx11
0*3f8 <conv\+0x38> 0e ?05 ?74 ?80 ? *	cfcvt32seq	mvf7, ?mvfx5
0*3fc <conv\+0x3c> ee ?0c ?34 ?a0 ? *	cfcvt32d	mvd3, ?mvfx12
0*400 <conv\+0x40> ae ?08 ?14 ?a0 ? *	cfcvt32dge	mvd1, ?mvfx8
0*404 <conv\+0x44> ee ?06 ?d4 ?a0 ? *	cfcvt32d	mvd13, ?mvfx6
0*408 <conv\+0x48> be ?02 ?44 ?a0 ? *	cfcvt32dlt	mvd4, ?mvfx2
0*40c <conv\+0x4c> 9e ?05 ?04 ?a0 ? *	cfcvt32dls	mvd0, ?mvfx5
0*410 <conv\+0x50> ee ?09 ?a4 ?c0 ? *	cfcvt64s	mvf10, ?mvdx9
0*414 <conv\+0x54> 4e ?03 ?e4 ?c0 ? *	cfcvt64smi	mvf14, ?mvdx3
0*418 <conv\+0x58> 8e ?07 ?d4 ?c0 ? *	cfcvt64shi	mvf13, ?mvdx7
0*41c <conv\+0x5c> 2e ?0c ?14 ?c0 ? *	cfcvt64scs	mvf1, ?mvdx12
0*420 <conv\+0x60> 6e ?00 ?b4 ?c0 ? *	cfcvt64svs	mvf11, ?mvdx0
0*424 <conv\+0x64> 7e ?0e ?54 ?e0 ? *	cfcvt64dvc	mvd5, ?mvdx14
0*428 <conv\+0x68> 3e ?0a ?c4 ?e0 ? *	cfcvt64dcc	mvd12, ?mvdx10
0*42c <conv\+0x6c> 1e ?0f ?84 ?e0 ? *	cfcvt64dne	mvd8, ?mvdx15
0*430 <conv\+0x70> de ?0b ?64 ?e0 ? *	cfcvt64dle	mvd6, ?mvdx11
0*434 <conv\+0x74> 4e ?09 ?24 ?e0 ? *	cfcvt64dmi	mvd2, ?mvdx9
0*438 <conv\+0x78> 0e ?1f ?55 ?80 ? *	cfcvts32eq	mvfx5, ?mvf15
0*43c <conv\+0x7c> ae ?14 ?95 ?80 ? *	cfcvts32ge	mvfx9, ?mvf4
0*440 <conv\+0x80> ee ?18 ?35 ?80 ? *	cfcvts32	mvfx3, ?mvf8
0*444 <conv\+0x84> de ?12 ?75 ?80 ? *	cfcvts32le	mvfx7, ?mvf2
0*448 <conv\+0x88> 1e ?16 ?c5 ?80 ? *	cfcvts32ne	mvfx12, ?mvf6
0*44c <conv\+0x8c> be ?17 ?05 ?a0 ? *	cfcvtd32lt	mvfx0, ?mvd7
0*450 <conv\+0x90> 5e ?13 ?e5 ?a0 ? *	cfcvtd32pl	mvfx14, ?mvd3
0*454 <conv\+0x94> ce ?11 ?a5 ?a0 ? *	cfcvtd32gt	mvfx10, ?mvd1
0*458 <conv\+0x98> 8e ?1d ?f5 ?a0 ? *	cfcvtd32hi	mvfx15, ?mvd13
0*45c <conv\+0x9c> 6e ?14 ?b5 ?a0 ? *	cfcvtd32vs	mvfx11, ?mvd4
0*460 <conv\+0xa0> 2e ?10 ?95 ?c0 ? *	cftruncs32cs	mvfx9, ?mvf0
0*464 <conv\+0xa4> 5e ?1a ?f5 ?c0 ? *	cftruncs32pl	mvfx15, ?mvf10
0*468 <conv\+0xa8> 9e ?1e ?45 ?c0 ? *	cftruncs32ls	mvfx4, ?mvf14
0*46c <conv\+0xac> 3e ?1d ?85 ?c0 ? *	cftruncs32cc	mvfx8, ?mvf13
0*470 <conv\+0xb0> 7e ?11 ?25 ?c0 ? *	cftruncs32vc	mvfx2, ?mvf1
0*474 <conv\+0xb4> ce ?1b ?65 ?e0 ? *	cftruncd32gt	mvfx6, ?mvd11
0*478 <conv\+0xb8> 0e ?15 ?75 ?e0 ? *	cftruncd32eq	mvfx7, ?mvd5
0*47c <conv\+0xbc> ee ?1c ?35 ?e0 ? *	cftruncd32	mvfx3, ?mvd12
0*480 <conv\+0xc0> ae ?18 ?15 ?e0 ? *	cftruncd32ge	mvfx1, ?mvd8
0*484 <conv\+0xc4> ee ?16 ?d5 ?e0 ? *	cftruncd32	mvfx13, ?mvd6
# shift:
0*488 <shift> be ?04 ?35 ?52 ? *	cfrshl32lt	mvfx4, ?mvfx2, ?r3
0*48c <shift\+0x4> 5e ?0f ?45 ?5a ? *	cfrshl32pl	mvfx15, ?mvfx10, ?r4
0*490 <shift\+0x8> ee ?03 ?25 ?58 ? *	cfrshl32	mvfx3, ?mvfx8, ?r2
0*494 <shift\+0xc> 2e ?01 ?95 ?5c ? *	cfrshl32cs	mvfx1, ?mvfx12, ?r9
0*498 <shift\+0x10> 0e ?07 ?75 ?55 ? *	cfrshl32eq	mvfx7, ?mvfx5, ?r7
0*49c <shift\+0x14> ce ?0a ?85 ?71 ? *	cfrshl64gt	mvdx10, ?mvdx1, ?r8
0*4a0 <shift\+0x18> de ?06 ?65 ?7b ? *	cfrshl64le	mvdx6, ?mvdx11, ?r6
0*4a4 <shift\+0x1c> 9e ?00 ?d5 ?75 ? *	cfrshl64ls	mvdx0, ?mvdx5, ?sp
0*4a8 <shift\+0x20> 9e ?04 ?b5 ?7e ? *	cfrshl64ls	mvdx4, ?mvdx14, ?fp
0*4ac <shift\+0x24> de ?07 ?c5 ?72 ? *	cfrshl64le	mvdx7, ?mvdx2, ?ip
0*4b0 <shift\+0x28> 6e ?00 ?b5 ?ef ? *	cfsh32vs	mvfx11, ?mvfx0, ?#-1
0*4b4 <shift\+0x2c> ee ?0c ?35 ?28 ? *	cfsh32	mvfx3, ?mvfx12, ?#24
0*4b8 <shift\+0x30> 8e ?0d ?f5 ?41 ? *	cfsh32hi	mvfx15, ?mvfx13, ?#33
0*4bc <shift\+0x34> 4e ?09 ?25 ?00 ? *	cfsh32mi	mvfx2, ?mvfx9, ?#0
0*4c0 <shift\+0x38> ee ?09 ?a5 ?40 ? *	cfsh32	mvfx10, ?mvfx9, ?#32
0*4c4 <shift\+0x3c> 3e ?2d ?85 ?c1 ? *	cfsh64cc	mvdx8, ?mvdx13, ?#-31
0*4c8 <shift\+0x40> 1e ?26 ?c5 ?01 ? *	cfsh64ne	mvdx12, ?mvdx6, ?#1
0*4cc <shift\+0x44> 7e ?2e ?55 ?c0 ? *	cfsh64vc	mvdx5, ?mvdx14, ?#-32
0*4d0 <shift\+0x48> ae ?28 ?15 ?c5 ? *	cfsh64ge	mvdx1, ?mvdx8, ?#-27
0*4d4 <shift\+0x4c> 6e ?24 ?b5 ?eb ? *	cfsh64vs	mvdx11, ?mvdx4, ?#-5
# comp:
0*4d8 <comp> 0e ?1f ?a4 ?9a ? *	cfcmpseq	sl, ?mvf15, ?mvf10
0*4dc <comp\+0x4> 4e ?13 ?14 ?98 ? *	cfcmpsmi	r1, ?mvf3, ?mvf8
0*4e0 <comp\+0x8> 7e ?11 ?f4 ?9c ? *	cfcmpsvc	pc, ?mvf1, ?mvf12
0*4e4 <comp\+0xc> be ?17 ?04 ?95 ? *	cfcmpslt	r0, ?mvf7, ?mvf5
0*4e8 <comp\+0x10> 3e ?1a ?e4 ?91 ? *	cfcmpscc	lr, ?mvf10, ?mvf1
0*4ec <comp\+0x14> ee ?16 ?54 ?bb ? *	cfcmpd	r5, ?mvd6, ?mvd11
0*4f0 <comp\+0x18> 2e ?10 ?34 ?b5 ? *	cfcmpdcs	r3, ?mvd0, ?mvd5
0*4f4 <comp\+0x1c> ae ?14 ?44 ?be ? *	cfcmpdge	r4, ?mvd4, ?mvd14
0*4f8 <comp\+0x20> 8e ?17 ?24 ?b2 ? *	cfcmpdhi	r2, ?mvd7, ?mvd2
0*4fc <comp\+0x24> ce ?1b ?94 ?b0 ? *	cfcmpdgt	r9, ?mvd11, ?mvd0
0*500 <comp\+0x28> 5e ?13 ?75 ?9c ? *	cfcmp32pl	r7, ?mvfx3, ?mvfx12
0*504 <comp\+0x2c> 1e ?1f ?85 ?9d ? *	cfcmp32ne	r8, ?mvfx15, ?mvfx13
0*508 <comp\+0x30> be ?12 ?65 ?99 ? *	cfcmp32lt	r6, ?mvfx2, ?mvfx9
0*50c <comp\+0x34> 5e ?1a ?d5 ?99 ? *	cfcmp32pl	sp, ?mvfx10, ?mvfx9
0*510 <comp\+0x38> ee ?18 ?b5 ?9d ? *	cfcmp32	fp, ?mvfx8, ?mvfx13
0*514 <comp\+0x3c> 2e ?1c ?c5 ?b6 ? *	cfcmp64cs	ip, ?mvdx12, ?mvdx6
0*518 <comp\+0x40> 0e ?15 ?a5 ?be ? *	cfcmp64eq	sl, ?mvdx5, ?mvdx14
0*51c <comp\+0x44> ce ?11 ?15 ?b8 ? *	cfcmp64gt	r1, ?mvdx1, ?mvdx8
0*520 <comp\+0x48> de ?1b ?f5 ?b4 ? *	cfcmp64le	pc, ?mvdx11, ?mvdx4
0*524 <comp\+0x4c> 9e ?15 ?05 ?bf ? *	cfcmp64ls	r0, ?mvdx5, ?mvdx15
# fp_arith:
0*528 <fp_arith> 9e ?3e ?44 ?00 ? *	cfabssls	mvf4, ?mvf14
0*52c <fp_arith\+0x4> 3e ?3d ?84 ?00 ? *	cfabsscc	mvf8, ?mvf13
0*530 <fp_arith\+0x8> 7e ?31 ?24 ?00 ? *	cfabssvc	mvf2, ?mvf1
0*534 <fp_arith\+0xc> ce ?3b ?64 ?00 ? *	cfabssgt	mvf6, ?mvf11
0*538 <fp_arith\+0x10> 0e ?35 ?74 ?00 ? *	cfabsseq	mvf7, ?mvf5
0*53c <fp_arith\+0x14> ee ?3c ?34 ?20 ? *	cfabsd	mvd3, ?mvd12
0*540 <fp_arith\+0x18> ae ?38 ?14 ?20 ? *	cfabsdge	mvd1, ?mvd8
0*544 <fp_arith\+0x1c> ee ?36 ?d4 ?20 ? *	cfabsd	mvd13, ?mvd6
0*548 <fp_arith\+0x20> be ?32 ?44 ?20 ? *	cfabsdlt	mvd4, ?mvd2
0*54c <fp_arith\+0x24> 9e ?35 ?04 ?20 ? *	cfabsdls	mvd0, ?mvd5
0*550 <fp_arith\+0x28> ee ?39 ?a4 ?40 ? *	cfnegs	mvf10, ?mvf9
0*554 <fp_arith\+0x2c> 4e ?33 ?e4 ?40 ? *	cfnegsmi	mvf14, ?mvf3
0*558 <fp_arith\+0x30> 8e ?37 ?d4 ?40 ? *	cfnegshi	mvf13, ?mvf7
0*55c <fp_arith\+0x34> 2e ?3c ?14 ?40 ? *	cfnegscs	mvf1, ?mvf12
0*560 <fp_arith\+0x38> 6e ?30 ?b4 ?40 ? *	cfnegsvs	mvf11, ?mvf0
0*564 <fp_arith\+0x3c> 7e ?3e ?54 ?60 ? *	cfnegdvc	mvd5, ?mvd14
0*568 <fp_arith\+0x40> 3e ?3a ?c4 ?60 ? *	cfnegdcc	mvd12, ?mvd10
0*56c <fp_arith\+0x44> 1e ?3f ?84 ?60 ? *	cfnegdne	mvd8, ?mvd15
0*570 <fp_arith\+0x48> de ?3b ?64 ?60 ? *	cfnegdle	mvd6, ?mvd11
0*574 <fp_arith\+0x4c> 4e ?39 ?24 ?60 ? *	cfnegdmi	mvd2, ?mvd9
0*578 <fp_arith\+0x50> 0e ?3f ?54 ?8a ? *	cfaddseq	mvf5, ?mvf15, ?mvf10
0*57c <fp_arith\+0x54> 4e ?33 ?e4 ?88 ? *	cfaddsmi	mvf14, ?mvf3, ?mvf8
0*580 <fp_arith\+0x58> 7e ?31 ?24 ?8c ? *	cfaddsvc	mvf2, ?mvf1, ?mvf12
0*584 <fp_arith\+0x5c> be ?37 ?04 ?85 ? *	cfaddslt	mvf0, ?mvf7, ?mvf5
0*588 <fp_arith\+0x60> 3e ?3a ?c4 ?81 ? *	cfaddscc	mvf12, ?mvf10, ?mvf1
0*58c <fp_arith\+0x64> ee ?36 ?d4 ?ab ? *	cfaddd	mvd13, ?mvd6, ?mvd11
0*590 <fp_arith\+0x68> 2e ?30 ?94 ?a5 ? *	cfadddcs	mvd9, ?mvd0, ?mvd5
0*594 <fp_arith\+0x6c> ae ?34 ?94 ?ae ? *	cfadddge	mvd9, ?mvd4, ?mvd14
0*598 <fp_arith\+0x70> 8e ?37 ?d4 ?a2 ? *	cfadddhi	mvd13, ?mvd7, ?mvd2
0*59c <fp_arith\+0x74> ce ?3b ?64 ?a0 ? *	cfadddgt	mvd6, ?mvd11, ?mvd0
0*5a0 <fp_arith\+0x78> 5e ?33 ?e4 ?cc ? *	cfsubspl	mvf14, ?mvf3, ?mvf12
0*5a4 <fp_arith\+0x7c> 1e ?3f ?84 ?cd ? *	cfsubsne	mvf8, ?mvf15, ?mvf13
0*5a8 <fp_arith\+0x80> be ?32 ?44 ?c9 ? *	cfsubslt	mvf4, ?mvf2, ?mvf9
0*5ac <fp_arith\+0x84> 5e ?3a ?f4 ?c9 ? *	cfsubspl	mvf15, ?mvf10, ?mvf9
0*5b0 <fp_arith\+0x88> ee ?38 ?34 ?cd ? *	cfsubs	mvf3, ?mvf8, ?mvf13
0*5b4 <fp_arith\+0x8c> 2e ?3c ?14 ?e6 ? *	cfsubdcs	mvd1, ?mvd12, ?mvd6
0*5b8 <fp_arith\+0x90> 0e ?35 ?74 ?ee ? *	cfsubdeq	mvd7, ?mvd5, ?mvd14
0*5bc <fp_arith\+0x94> ce ?31 ?a4 ?e8 ? *	cfsubdgt	mvd10, ?mvd1, ?mvd8
0*5c0 <fp_arith\+0x98> de ?3b ?64 ?e4 ? *	cfsubdle	mvd6, ?mvd11, ?mvd4
0*5c4 <fp_arith\+0x9c> 9e ?35 ?04 ?ef ? *	cfsubdls	mvd0, ?mvd5, ?mvd15
0*5c8 <fp_arith\+0xa0> 9e ?1e ?44 ?03 ? *	cfmulsls	mvf4, ?mvf14, ?mvf3
0*5cc <fp_arith\+0xa4> de ?12 ?74 ?01 ? *	cfmulsle	mvf7, ?mvf2, ?mvf1
0*5d0 <fp_arith\+0xa8> 6e ?10 ?b4 ?07 ? *	cfmulsvs	mvf11, ?mvf0, ?mvf7
0*5d4 <fp_arith\+0xac> ee ?1c ?34 ?0a ? *	cfmuls	mvf3, ?mvf12, ?mvf10
0*5d8 <fp_arith\+0xb0> 8e ?1d ?f4 ?06 ? *	cfmulshi	mvf15, ?mvf13, ?mvf6
0*5dc <fp_arith\+0xb4> 4e ?19 ?24 ?20 ? *	cfmuldmi	mvd2, ?mvd9, ?mvd0
0*5e0 <fp_arith\+0xb8> ee ?19 ?a4 ?24 ? *	cfmuld	mvd10, ?mvd9, ?mvd4
0*5e4 <fp_arith\+0xbc> 3e ?1d ?84 ?27 ? *	cfmuldcc	mvd8, ?mvd13, ?mvd7
0*5e8 <fp_arith\+0xc0> 1e ?16 ?c4 ?2b ? *	cfmuldne	mvd12, ?mvd6, ?mvd11
0*5ec <fp_arith\+0xc4> 7e ?1e ?54 ?23 ? *	cfmuldvc	mvd5, ?mvd14, ?mvd3
# int_arith:
0*5f0 <int_arith> ae ?38 ?15 ?00 ? *	cfabs32ge	mvfx1, ?mvfx8
0*5f4 <int_arith\+0x4> ee ?36 ?d5 ?00 ? *	cfabs32	mvfx13, ?mvfx6
0*5f8 <int_arith\+0x8> be ?32 ?45 ?00 ? *	cfabs32lt	mvfx4, ?mvfx2
0*5fc <int_arith\+0xc> 9e ?35 ?05 ?00 ? *	cfabs32ls	mvfx0, ?mvfx5
0*600 <int_arith\+0x10> ee ?39 ?a5 ?00 ? *	cfabs32	mvfx10, ?mvfx9
0*604 <int_arith\+0x14> 4e ?33 ?e5 ?20 ? *	cfabs64mi	mvdx14, ?mvdx3
0*608 <int_arith\+0x18> 8e ?37 ?d5 ?20 ? *	cfabs64hi	mvdx13, ?mvdx7
0*60c <int_arith\+0x1c> 2e ?3c ?15 ?20 ? *	cfabs64cs	mvdx1, ?mvdx12
0*610 <int_arith\+0x20> 6e ?30 ?b5 ?20 ? *	cfabs64vs	mvdx11, ?mvdx0
0*614 <int_arith\+0x24> 7e ?3e ?55 ?20 ? *	cfabs64vc	mvdx5, ?mvdx14
0*618 <int_arith\+0x28> 3e ?3a ?c5 ?40 ? *	cfneg32cc	mvfx12, ?mvfx10
0*61c <int_arith\+0x2c> 1e ?3f ?85 ?40 ? *	cfneg32ne	mvfx8, ?mvfx15
0*620 <int_arith\+0x30> de ?3b ?65 ?40 ? *	cfneg32le	mvfx6, ?mvfx11
0*624 <int_arith\+0x34> 4e ?39 ?25 ?40 ? *	cfneg32mi	mvfx2, ?mvfx9
0*628 <int_arith\+0x38> 0e ?3f ?55 ?40 ? *	cfneg32eq	mvfx5, ?mvfx15
0*62c <int_arith\+0x3c> ae ?34 ?95 ?60 ? *	cfneg64ge	mvdx9, ?mvdx4
0*630 <int_arith\+0x40> ee ?38 ?35 ?60 ? *	cfneg64	mvdx3, ?mvdx8
0*634 <int_arith\+0x44> de ?32 ?75 ?60 ? *	cfneg64le	mvdx7, ?mvdx2
0*638 <int_arith\+0x48> 1e ?36 ?c5 ?60 ? *	cfneg64ne	mvdx12, ?mvdx6
0*63c <int_arith\+0x4c> be ?37 ?05 ?60 ? *	cfneg64lt	mvdx0, ?mvdx7
0*640 <int_arith\+0x50> 5e ?33 ?e5 ?8c ? *	cfadd32pl	mvfx14, ?mvfx3, ?mvfx12
0*644 <int_arith\+0x54> 1e ?3f ?85 ?8d ? *	cfadd32ne	mvfx8, ?mvfx15, ?mvfx13
0*648 <int_arith\+0x58> be ?32 ?45 ?89 ? *	cfadd32lt	mvfx4, ?mvfx2, ?mvfx9
0*64c <int_arith\+0x5c> 5e ?3a ?f5 ?89 ? *	cfadd32pl	mvfx15, ?mvfx10, ?mvfx9
0*650 <int_arith\+0x60> ee ?38 ?35 ?8d ? *	cfadd32	mvfx3, ?mvfx8, ?mvfx13
0*654 <int_arith\+0x64> 2e ?3c ?15 ?a6 ? *	cfadd64cs	mvdx1, ?mvdx12, ?mvdx6
0*658 <int_arith\+0x68> 0e ?35 ?75 ?ae ? *	cfadd64eq	mvdx7, ?mvdx5, ?mvdx14
0*65c <int_arith\+0x6c> ce ?31 ?a5 ?a8 ? *	cfadd64gt	mvdx10, ?mvdx1, ?mvdx8
0*660 <int_arith\+0x70> de ?3b ?65 ?a4 ? *	cfadd64le	mvdx6, ?mvdx11, ?mvdx4
0*664 <int_arith\+0x74> 9e ?35 ?05 ?af ? *	cfadd64ls	mvdx0, ?mvdx5, ?mvdx15
0*668 <int_arith\+0x78> 9e ?3e ?45 ?c3 ? *	cfsub32ls	mvfx4, ?mvfx14, ?mvfx3
0*66c <int_arith\+0x7c> de ?32 ?75 ?c1 ? *	cfsub32le	mvfx7, ?mvfx2, ?mvfx1
0*670 <int_arith\+0x80> 6e ?30 ?b5 ?c7 ? *	cfsub32vs	mvfx11, ?mvfx0, ?mvfx7
0*674 <int_arith\+0x84> ee ?3c ?35 ?ca ? *	cfsub32	mvfx3, ?mvfx12, ?mvfx10
0*678 <int_arith\+0x88> 8e ?3d ?f5 ?c6 ? *	cfsub32hi	mvfx15, ?mvfx13, ?mvfx6
0*67c <int_arith\+0x8c> 4e ?39 ?25 ?e0 ? *	cfsub64mi	mvdx2, ?mvdx9, ?mvdx0
0*680 <int_arith\+0x90> ee ?39 ?a5 ?e4 ? *	cfsub64	mvdx10, ?mvdx9, ?mvdx4
0*684 <int_arith\+0x94> 3e ?3d ?85 ?e7 ? *	cfsub64cc	mvdx8, ?mvdx13, ?mvdx7
0*688 <int_arith\+0x98> 1e ?36 ?c5 ?eb ? *	cfsub64ne	mvdx12, ?mvdx6, ?mvdx11
0*68c <int_arith\+0x9c> 7e ?3e ?55 ?e3 ? *	cfsub64vc	mvdx5, ?mvdx14, ?mvdx3
0*690 <int_arith\+0xa0> ae ?18 ?15 ?0f ? *	cfmul32ge	mvfx1, ?mvfx8, ?mvfx15
0*694 <int_arith\+0xa4> 6e ?14 ?b5 ?02 ? *	cfmul32vs	mvfx11, ?mvfx4, ?mvfx2
0*698 <int_arith\+0xa8> 0e ?1f ?55 ?0a ? *	cfmul32eq	mvfx5, ?mvfx15, ?mvfx10
0*69c <int_arith\+0xac> 4e ?13 ?e5 ?08 ? *	cfmul32mi	mvfx14, ?mvfx3, ?mvfx8
0*6a0 <int_arith\+0xb0> 7e ?11 ?25 ?0c ? *	cfmul32vc	mvfx2, ?mvfx1, ?mvfx12
0*6a4 <int_arith\+0xb4> be ?17 ?05 ?25 ? *	cfmul64lt	mvdx0, ?mvdx7, ?mvdx5
0*6a8 <int_arith\+0xb8> 3e ?1a ?c5 ?21 ? *	cfmul64cc	mvdx12, ?mvdx10, ?mvdx1
0*6ac <int_arith\+0xbc> ee ?16 ?d5 ?2b ? *	cfmul64	mvdx13, ?mvdx6, ?mvdx11
0*6b0 <int_arith\+0xc0> 2e ?10 ?95 ?25 ? *	cfmul64cs	mvdx9, ?mvdx0, ?mvdx5
0*6b4 <int_arith\+0xc4> ae ?14 ?95 ?2e ? *	cfmul64ge	mvdx9, ?mvdx4, ?mvdx14
0*6b8 <int_arith\+0xc8> 8e ?17 ?d5 ?42 ? *	cfmac32hi	mvfx13, ?mvfx7, ?mvfx2
0*6bc <int_arith\+0xcc> ce ?1b ?65 ?40 ? *	cfmac32gt	mvfx6, ?mvfx11, ?mvfx0
0*6c0 <int_arith\+0xd0> 5e ?13 ?e5 ?4c ? *	cfmac32pl	mvfx14, ?mvfx3, ?mvfx12
0*6c4 <int_arith\+0xd4> 1e ?1f ?85 ?4d ? *	cfmac32ne	mvfx8, ?mvfx15, ?mvfx13
0*6c8 <int_arith\+0xd8> be ?12 ?45 ?49 ? *	cfmac32lt	mvfx4, ?mvfx2, ?mvfx9
0*6cc <int_arith\+0xdc> 5e ?1a ?f5 ?69 ? *	cfmsc32pl	mvfx15, ?mvfx10, ?mvfx9
0*6d0 <int_arith\+0xe0> ee ?18 ?35 ?6d ? *	cfmsc32	mvfx3, ?mvfx8, ?mvfx13
0*6d4 <int_arith\+0xe4> 2e ?1c ?15 ?66 ? *	cfmsc32cs	mvfx1, ?mvfx12, ?mvfx6
0*6d8 <int_arith\+0xe8> 0e ?15 ?75 ?6e ? *	cfmsc32eq	mvfx7, ?mvfx5, ?mvfx14
0*6dc <int_arith\+0xec> ce ?11 ?a5 ?68 ? *	cfmsc32gt	mvfx10, ?mvfx1, ?mvfx8
# acc_arith:
0*6e0 <acc_arith> de ?04 ?b6 ?02 ? *	cfmadd32le	mvax0, ?mvfx11, ?mvfx4, ?mvfx2
0*6e4 <acc_arith\+0x4> 9e ?0f ?56 ?0a ? *	cfmadd32ls	mvax0, ?mvfx5, ?mvfx15, ?mvfx10
0*6e8 <acc_arith\+0x8> 9e ?03 ?e6 ?08 ? *	cfmadd32ls	mvax0, ?mvfx14, ?mvfx3, ?mvfx8
0*6ec <acc_arith\+0xc> de ?01 ?26 ?4c ? *	cfmadd32le	mvax2, ?mvfx2, ?mvfx1, ?mvfx12
0*6f0 <acc_arith\+0x10> 6e ?07 ?06 ?25 ? *	cfmadd32vs	mvax1, ?mvfx0, ?mvfx7, ?mvfx5
0*6f4 <acc_arith\+0x14> ee ?1a ?c6 ?41 ? *	cfmsub32	mvax2, ?mvfx12, ?mvfx10, ?mvfx1
0*6f8 <acc_arith\+0x18> 8e ?16 ?d6 ?6b ? *	cfmsub32hi	mvax3, ?mvfx13, ?mvfx6, ?mvfx11
0*6fc <acc_arith\+0x1c> 4e ?10 ?96 ?05 ? *	cfmsub32mi	mvax0, ?mvfx9, ?mvfx0, ?mvfx5
0*700 <acc_arith\+0x20> ee ?14 ?96 ?4e ? *	cfmsub32	mvax2, ?mvfx9, ?mvfx4, ?mvfx14
0*704 <acc_arith\+0x24> 3e ?17 ?d6 ?22 ? *	cfmsub32cc	mvax1, ?mvfx13, ?mvfx7, ?mvfx2
0*708 <acc_arith\+0x28> 1e ?2b ?06 ?40 ? *	cfmadda32ne	mvax2, ?mvax0, ?mvfx11, ?mvfx0
0*70c <acc_arith\+0x2c> 7e ?23 ?26 ?6c ? *	cfmadda32vc	mvax3, ?mvax2, ?mvfx3, ?mvfx12
0*710 <acc_arith\+0x30> ae ?2f ?16 ?6d ? *	cfmadda32ge	mvax3, ?mvax1, ?mvfx15, ?mvfx13
0*714 <acc_arith\+0x34> 6e ?22 ?26 ?69 ? *	cfmadda32vs	mvax3, ?mvax2, ?mvfx2, ?mvfx9
0*718 <acc_arith\+0x38> 0e ?2a ?36 ?29 ? *	cfmadda32eq	mvax1, ?mvax3, ?mvfx10, ?mvfx9
0*71c <acc_arith\+0x3c> 4e ?38 ?36 ?2d ? *	cfmsuba32mi	mvax1, ?mvax3, ?mvfx8, ?mvfx13
0*720 <acc_arith\+0x40> 7e ?3c ?36 ?06 ? *	cfmsuba32vc	mvax0, ?mvax3, ?mvfx12, ?mvfx6
0*724 <acc_arith\+0x44> be ?35 ?16 ?0e ? *	cfmsuba32lt	mvax0, ?mvax1, ?mvfx5, ?mvfx14
0*728 <acc_arith\+0x48> 3e ?31 ?16 ?08 ? *	cfmsuba32cc	mvax0, ?mvax1, ?mvfx1, ?mvfx8
0*72c <acc_arith\+0x4c> ee ?3b ?06 ?44 ? *	cfmsuba32	mvax2, ?mvax0, ?mvfx11, ?mvfx4
