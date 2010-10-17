#objdump: -dr --prefix-address --show-raw-insn
#name: Maverick
#as: -mcpu=ep9312

# Test the instructions of the Cirrus Maverick floating point co-processor

.*: +file format.*arm.*

Disassembly of section .text:
# load_store:
0*0 <load_store> 0d ?9d ?54 ?3f ? *	cfldrseq	mvf5, ?\[sp, #252\]
0*4 <load_store\+0x4> 4d ?9b ?e4 ?12 ? *	cfldrsmi	mvf14, ?\[fp, #72\]
0*8 <load_store\+0x8> 7d ?1c ?24 ?3c ? *	cfldrsvc	mvf2, ?\[ip, #-240\]
0*c <load_store\+0xc> bd ?9a ?04 ?3f ? *	cfldrslt	mvf0, ?\[sl, #252\]
0*10 <load_store\+0x10> cd ?9b ?a4 ?12 ? *	cfldrsgt	mvf10, ?\[fp, #72\]
0*14 <load_store\+0x14> dd ?3c ?64 ?3c ? *	cfldrsle	mvf6, ?\[ip, #-240\]!
0*18 <load_store\+0x18> 9d ?ba ?04 ?3f ? *	cfldrsls	mvf0, ?\[sl, #252\]!
0*1c <load_store\+0x1c> 4d ?bb ?e4 ?12 ? *	cfldrsmi	mvf14, ?\[fp, #72\]!
0*20 <load_store\+0x20> 7d ?3c ?24 ?3c ? *	cfldrsvc	mvf2, ?\[ip, #-240\]!
0*24 <load_store\+0x24> bd ?ba ?04 ?3f ? *	cfldrslt	mvf0, ?\[sl, #252\]!
0*28 <load_store\+0x28> cc ?bb ?a4 ?12 ? *	cfldrsgt	mvf10, ?\[fp\], #72
0*2c <load_store\+0x2c> dc ?3c ?64 ?3c ? *	cfldrsle	mvf6, ?\[ip\], #-240
0*30 <load_store\+0x30> 9c ?ba ?04 ?3f ? *	cfldrsls	mvf0, ?\[sl\], #252
0*34 <load_store\+0x34> 4c ?bb ?e4 ?12 ? *	cfldrsmi	mvf14, ?\[fp\], #72
0*38 <load_store\+0x38> 7c ?3c ?24 ?3c ? *	cfldrsvc	mvf2, ?\[ip\], #-240
0*3c <load_store\+0x3c> bd ?da ?04 ?3f ? *	cfldrdlt	mvd0, ?\[sl, #252\]
0*40 <load_store\+0x40> cd ?db ?a4 ?12 ? *	cfldrdgt	mvd10, ?\[fp, #72\]
0*44 <load_store\+0x44> dd ?5c ?64 ?3c ? *	cfldrdle	mvd6, ?\[ip, #-240\]
0*48 <load_store\+0x48> 9d ?da ?04 ?3f ? *	cfldrdls	mvd0, ?\[sl, #252\]
0*4c <load_store\+0x4c> 4d ?db ?e4 ?12 ? *	cfldrdmi	mvd14, ?\[fp, #72\]
0*50 <load_store\+0x50> 7d ?7c ?24 ?3c ? *	cfldrdvc	mvd2, ?\[ip, #-240\]!
0*54 <load_store\+0x54> bd ?fa ?04 ?3f ? *	cfldrdlt	mvd0, ?\[sl, #252\]!
0*58 <load_store\+0x58> cd ?fb ?a4 ?12 ? *	cfldrdgt	mvd10, ?\[fp, #72\]!
0*5c <load_store\+0x5c> dd ?7c ?64 ?3c ? *	cfldrdle	mvd6, ?\[ip, #-240\]!
0*60 <load_store\+0x60> 9d ?fa ?04 ?3f ? *	cfldrdls	mvd0, ?\[sl, #252\]!
0*64 <load_store\+0x64> 4c ?fb ?e4 ?12 ? *	cfldrdmi	mvd14, ?\[fp\], #72
0*68 <load_store\+0x68> 7c ?7c ?24 ?3c ? *	cfldrdvc	mvd2, ?\[ip\], #-240
0*6c <load_store\+0x6c> bc ?fa ?04 ?3f ? *	cfldrdlt	mvd0, ?\[sl\], #252
0*70 <load_store\+0x70> cc ?fb ?a4 ?12 ? *	cfldrdgt	mvd10, ?\[fp\], #72
0*74 <load_store\+0x74> dc ?7c ?64 ?3c ? *	cfldrdle	mvd6, ?\[ip\], #-240
0*78 <load_store\+0x78> 9d ?9a ?05 ?3f ? *	cfldr32ls	mvfx0, ?\[sl, #252\]
0*7c <load_store\+0x7c> 4d ?9b ?e5 ?12 ? *	cfldr32mi	mvfx14, ?\[fp, #72\]
0*80 <load_store\+0x80> 7d ?1c ?25 ?3c ? *	cfldr32vc	mvfx2, ?\[ip, #-240\]
0*84 <load_store\+0x84> bd ?9a ?05 ?3f ? *	cfldr32lt	mvfx0, ?\[sl, #252\]
0*88 <load_store\+0x88> cd ?9b ?a5 ?12 ? *	cfldr32gt	mvfx10, ?\[fp, #72\]
0*8c <load_store\+0x8c> dd ?3c ?65 ?3c ? *	cfldr32le	mvfx6, ?\[ip, #-240\]!
0*90 <load_store\+0x90> 9d ?ba ?05 ?3f ? *	cfldr32ls	mvfx0, ?\[sl, #252\]!
0*94 <load_store\+0x94> 4d ?bb ?e5 ?12 ? *	cfldr32mi	mvfx14, ?\[fp, #72\]!
0*98 <load_store\+0x98> 7d ?3c ?25 ?3c ? *	cfldr32vc	mvfx2, ?\[ip, #-240\]!
0*9c <load_store\+0x9c> bd ?ba ?05 ?3f ? *	cfldr32lt	mvfx0, ?\[sl, #252\]!
0*a0 <load_store\+0xa0> cc ?bb ?a5 ?12 ? *	cfldr32gt	mvfx10, ?\[fp\], #72
0*a4 <load_store\+0xa4> dc ?3c ?65 ?3c ? *	cfldr32le	mvfx6, ?\[ip\], #-240
0*a8 <load_store\+0xa8> 9c ?ba ?05 ?3f ? *	cfldr32ls	mvfx0, ?\[sl\], #252
0*ac <load_store\+0xac> 4c ?bb ?e5 ?12 ? *	cfldr32mi	mvfx14, ?\[fp\], #72
0*b0 <load_store\+0xb0> 7c ?3c ?25 ?3c ? *	cfldr32vc	mvfx2, ?\[ip\], #-240
0*b4 <load_store\+0xb4> bd ?da ?05 ?3f ? *	cfldr64lt	mvdx0, ?\[sl, #252\]
0*b8 <load_store\+0xb8> cd ?db ?a5 ?12 ? *	cfldr64gt	mvdx10, ?\[fp, #72\]
0*bc <load_store\+0xbc> dd ?5c ?65 ?3c ? *	cfldr64le	mvdx6, ?\[ip, #-240\]
0*c0 <load_store\+0xc0> 9d ?da ?05 ?3f ? *	cfldr64ls	mvdx0, ?\[sl, #252\]
0*c4 <load_store\+0xc4> 4d ?db ?e5 ?12 ? *	cfldr64mi	mvdx14, ?\[fp, #72\]
0*c8 <load_store\+0xc8> 7d ?7c ?25 ?3c ? *	cfldr64vc	mvdx2, ?\[ip, #-240\]!
0*cc <load_store\+0xcc> bd ?fa ?05 ?3f ? *	cfldr64lt	mvdx0, ?\[sl, #252\]!
0*d0 <load_store\+0xd0> cd ?fb ?a5 ?12 ? *	cfldr64gt	mvdx10, ?\[fp, #72\]!
0*d4 <load_store\+0xd4> dd ?7c ?65 ?3c ? *	cfldr64le	mvdx6, ?\[ip, #-240\]!
0*d8 <load_store\+0xd8> 9d ?fa ?05 ?3f ? *	cfldr64ls	mvdx0, ?\[sl, #252\]!
0*dc <load_store\+0xdc> 4c ?fb ?e5 ?12 ? *	cfldr64mi	mvdx14, ?\[fp\], #72
0*e0 <load_store\+0xe0> 7c ?7c ?25 ?3c ? *	cfldr64vc	mvdx2, ?\[ip\], #-240
0*e4 <load_store\+0xe4> bc ?fa ?05 ?3f ? *	cfldr64lt	mvdx0, ?\[sl\], #252
0*e8 <load_store\+0xe8> cc ?fb ?a5 ?12 ? *	cfldr64gt	mvdx10, ?\[fp\], #72
0*ec <load_store\+0xec> dc ?7c ?65 ?3c ? *	cfldr64le	mvdx6, ?\[ip\], #-240
0*f0 <load_store\+0xf0> 9d ?8a ?04 ?3f ? *	cfstrsls	mvf0, ?\[sl, #252\]
0*f4 <load_store\+0xf4> 4d ?8b ?e4 ?12 ? *	cfstrsmi	mvf14, ?\[fp, #72\]
0*f8 <load_store\+0xf8> 7d ?0c ?24 ?3c ? *	cfstrsvc	mvf2, ?\[ip, #-240\]
0*fc <load_store\+0xfc> bd ?8a ?04 ?3f ? *	cfstrslt	mvf0, ?\[sl, #252\]
0*100 <load_store\+0x100> cd ?8b ?a4 ?12 ? *	cfstrsgt	mvf10, ?\[fp, #72\]
0*104 <load_store\+0x104> dd ?2c ?64 ?3c ? *	cfstrsle	mvf6, ?\[ip, #-240\]!
0*108 <load_store\+0x108> 9d ?aa ?04 ?3f ? *	cfstrsls	mvf0, ?\[sl, #252\]!
0*10c <load_store\+0x10c> 4d ?ab ?e4 ?12 ? *	cfstrsmi	mvf14, ?\[fp, #72\]!
0*110 <load_store\+0x110> 7d ?2c ?24 ?3c ? *	cfstrsvc	mvf2, ?\[ip, #-240\]!
0*114 <load_store\+0x114> bd ?aa ?04 ?3f ? *	cfstrslt	mvf0, ?\[sl, #252\]!
0*118 <load_store\+0x118> cc ?ab ?a4 ?12 ? *	cfstrsgt	mvf10, ?\[fp\], #72
0*11c <load_store\+0x11c> dc ?2c ?64 ?3c ? *	cfstrsle	mvf6, ?\[ip\], #-240
0*120 <load_store\+0x120> 9c ?aa ?04 ?3f ? *	cfstrsls	mvf0, ?\[sl\], #252
0*124 <load_store\+0x124> 4c ?ab ?e4 ?12 ? *	cfstrsmi	mvf14, ?\[fp\], #72
0*128 <load_store\+0x128> 7c ?2c ?24 ?3c ? *	cfstrsvc	mvf2, ?\[ip\], #-240
0*12c <load_store\+0x12c> bd ?ca ?04 ?3f ? *	cfstrdlt	mvd0, ?\[sl, #252\]
0*130 <load_store\+0x130> cd ?cb ?a4 ?12 ? *	cfstrdgt	mvd10, ?\[fp, #72\]
0*134 <load_store\+0x134> dd ?4c ?64 ?3c ? *	cfstrdle	mvd6, ?\[ip, #-240\]
0*138 <load_store\+0x138> 9d ?ca ?04 ?3f ? *	cfstrdls	mvd0, ?\[sl, #252\]
0*13c <load_store\+0x13c> 4d ?cb ?e4 ?12 ? *	cfstrdmi	mvd14, ?\[fp, #72\]
0*140 <load_store\+0x140> 7d ?6c ?24 ?3c ? *	cfstrdvc	mvd2, ?\[ip, #-240\]!
0*144 <load_store\+0x144> bd ?ea ?04 ?3f ? *	cfstrdlt	mvd0, ?\[sl, #252\]!
0*148 <load_store\+0x148> cd ?eb ?a4 ?12 ? *	cfstrdgt	mvd10, ?\[fp, #72\]!
0*14c <load_store\+0x14c> dd ?6c ?64 ?3c ? *	cfstrdle	mvd6, ?\[ip, #-240\]!
0*150 <load_store\+0x150> 9d ?ea ?04 ?3f ? *	cfstrdls	mvd0, ?\[sl, #252\]!
0*154 <load_store\+0x154> 4c ?eb ?e4 ?12 ? *	cfstrdmi	mvd14, ?\[fp\], #72
0*158 <load_store\+0x158> 7c ?6c ?24 ?3c ? *	cfstrdvc	mvd2, ?\[ip\], #-240
0*15c <load_store\+0x15c> bc ?ea ?04 ?3f ? *	cfstrdlt	mvd0, ?\[sl\], #252
0*160 <load_store\+0x160> cc ?eb ?a4 ?12 ? *	cfstrdgt	mvd10, ?\[fp\], #72
0*164 <load_store\+0x164> dc ?6c ?64 ?3c ? *	cfstrdle	mvd6, ?\[ip\], #-240
0*168 <load_store\+0x168> 9d ?8a ?05 ?3f ? *	cfstr32ls	mvfx0, ?\[sl, #252\]
0*16c <load_store\+0x16c> 4d ?8b ?e5 ?12 ? *	cfstr32mi	mvfx14, ?\[fp, #72\]
0*170 <load_store\+0x170> 7d ?0c ?25 ?3c ? *	cfstr32vc	mvfx2, ?\[ip, #-240\]
0*174 <load_store\+0x174> bd ?8a ?05 ?3f ? *	cfstr32lt	mvfx0, ?\[sl, #252\]
0*178 <load_store\+0x178> cd ?8b ?a5 ?12 ? *	cfstr32gt	mvfx10, ?\[fp, #72\]
0*17c <load_store\+0x17c> dd ?2c ?65 ?3c ? *	cfstr32le	mvfx6, ?\[ip, #-240\]!
0*180 <load_store\+0x180> 9d ?aa ?05 ?3f ? *	cfstr32ls	mvfx0, ?\[sl, #252\]!
0*184 <load_store\+0x184> 4d ?ab ?e5 ?12 ? *	cfstr32mi	mvfx14, ?\[fp, #72\]!
0*188 <load_store\+0x188> 7d ?2c ?25 ?3c ? *	cfstr32vc	mvfx2, ?\[ip, #-240\]!
0*18c <load_store\+0x18c> bd ?aa ?05 ?3f ? *	cfstr32lt	mvfx0, ?\[sl, #252\]!
0*190 <load_store\+0x190> cc ?ab ?a5 ?12 ? *	cfstr32gt	mvfx10, ?\[fp\], #72
0*194 <load_store\+0x194> dc ?2c ?65 ?3c ? *	cfstr32le	mvfx6, ?\[ip\], #-240
0*198 <load_store\+0x198> 9c ?aa ?05 ?3f ? *	cfstr32ls	mvfx0, ?\[sl\], #252
0*19c <load_store\+0x19c> 4c ?ab ?e5 ?12 ? *	cfstr32mi	mvfx14, ?\[fp\], #72
0*1a0 <load_store\+0x1a0> 7c ?2c ?25 ?3c ? *	cfstr32vc	mvfx2, ?\[ip\], #-240
0*1a4 <load_store\+0x1a4> bd ?ca ?05 ?3f ? *	cfstr64lt	mvdx0, ?\[sl, #252\]
0*1a8 <load_store\+0x1a8> cd ?cb ?a5 ?12 ? *	cfstr64gt	mvdx10, ?\[fp, #72\]
0*1ac <load_store\+0x1ac> dd ?4c ?65 ?3c ? *	cfstr64le	mvdx6, ?\[ip, #-240\]
0*1b0 <load_store\+0x1b0> 9d ?ca ?05 ?3f ? *	cfstr64ls	mvdx0, ?\[sl, #252\]
0*1b4 <load_store\+0x1b4> 4d ?cb ?e5 ?12 ? *	cfstr64mi	mvdx14, ?\[fp, #72\]
0*1b8 <load_store\+0x1b8> 7d ?6c ?25 ?3c ? *	cfstr64vc	mvdx2, ?\[ip, #-240\]!
0*1bc <load_store\+0x1bc> bd ?ea ?05 ?3f ? *	cfstr64lt	mvdx0, ?\[sl, #252\]!
0*1c0 <load_store\+0x1c0> cd ?eb ?a5 ?12 ? *	cfstr64gt	mvdx10, ?\[fp, #72\]!
0*1c4 <load_store\+0x1c4> dd ?6c ?65 ?3c ? *	cfstr64le	mvdx6, ?\[ip, #-240\]!
0*1c8 <load_store\+0x1c8> 9d ?ea ?05 ?3f ? *	cfstr64ls	mvdx0, ?\[sl, #252\]!
0*1cc <load_store\+0x1cc> 4c ?eb ?e5 ?12 ? *	cfstr64mi	mvdx14, ?\[fp\], #72
0*1d0 <load_store\+0x1d0> 7c ?6c ?25 ?3c ? *	cfstr64vc	mvdx2, ?\[ip\], #-240
0*1d4 <load_store\+0x1d4> bc ?ea ?05 ?3f ? *	cfstr64lt	mvdx0, ?\[sl\], #252
0*1d8 <load_store\+0x1d8> cc ?eb ?a5 ?12 ? *	cfstr64gt	mvdx10, ?\[fp\], #72
0*1dc <load_store\+0x1dc> dc ?6c ?65 ?3c ? *	cfstr64le	mvdx6, ?\[ip\], #-240
# move:
0*1e0 <move> 9e ?00 ?a4 ?50 ? *	cfmvsrls	mvf0, ?sl
0*1e4 <move\+0x4> ee ?0a ?44 ?50 ? *	cfmvsr	mvf10, ?r4
0*1e8 <move\+0x8> 4e ?0e ?b4 ?50 ? *	cfmvsrmi	mvf14, ?fp
0*1ec <move\+0xc> 8e ?0d ?54 ?50 ? *	cfmvsrhi	mvf13, ?r5
0*1f0 <move\+0x10> 2e ?01 ?64 ?50 ? *	cfmvsrcs	mvf1, ?r6
0*1f4 <move\+0x14> 6e ?10 ?34 ?50 ? *	cfmvrsvs	r3, ?mvf0
0*1f8 <move\+0x18> 7e ?1e ?d4 ?50 ? *	cfmvrsvc	sp, ?mvf14
0*1fc <move\+0x1c> 3e ?1a ?e4 ?50 ? *	cfmvrscc	lr, ?mvf10
0*200 <move\+0x20> 1e ?1f ?84 ?50 ? *	cfmvrsne	r8, ?mvf15
0*204 <move\+0x24> de ?1b ?f4 ?50 ? *	cfmvrsle	pc, ?mvf11
0*208 <move\+0x28> 4e ?02 ?34 ?10 ? *	cfmvdlrmi	mvd2, ?r3
0*20c <move\+0x2c> 0e ?05 ?d4 ?10 ? *	cfmvdlreq	mvd5, ?sp
0*210 <move\+0x30> ae ?09 ?e4 ?10 ? *	cfmvdlrge	mvd9, ?lr
0*214 <move\+0x34> ee ?03 ?84 ?10 ? *	cfmvdlr	mvd3, ?r8
0*218 <move\+0x38> de ?07 ?f4 ?10 ? *	cfmvdlrle	mvd7, ?pc
0*21c <move\+0x3c> 1e ?16 ?64 ?10 ? *	cfmvrdlne	r6, ?mvd6
0*220 <move\+0x40> be ?17 ?04 ?10 ? *	cfmvrdllt	r0, ?mvd7
0*224 <move\+0x44> 5e ?13 ?74 ?10 ? *	cfmvrdlpl	r7, ?mvd3
0*228 <move\+0x48> ce ?11 ?14 ?10 ? *	cfmvrdlgt	r1, ?mvd1
0*22c <move\+0x4c> 8e ?1d ?24 ?10 ? *	cfmvrdlhi	r2, ?mvd13
0*230 <move\+0x50> 6e ?0b ?64 ?30 ? *	cfmvdhrvs	mvd11, ?r6
0*234 <move\+0x54> 2e ?09 ?04 ?30 ? *	cfmvdhrcs	mvd9, ?r0
0*238 <move\+0x58> 5e ?0f ?74 ?30 ? *	cfmvdhrpl	mvd15, ?r7
0*23c <move\+0x5c> 9e ?04 ?14 ?30 ? *	cfmvdhrls	mvd4, ?r1
0*240 <move\+0x60> 3e ?08 ?24 ?30 ? *	cfmvdhrcc	mvd8, ?r2
0*244 <move\+0x64> 7e ?11 ?f4 ?30 ? *	cfmvrdhvc	pc, ?mvd1
0*248 <move\+0x68> ce ?1b ?94 ?30 ? *	cfmvrdhgt	r9, ?mvd11
0*24c <move\+0x6c> 0e ?15 ?a4 ?30 ? *	cfmvrdheq	sl, ?mvd5
0*250 <move\+0x70> ee ?1c ?44 ?30 ? *	cfmvrdh	r4, ?mvd12
0*254 <move\+0x74> ae ?18 ?b4 ?30 ? *	cfmvrdhge	fp, ?mvd8
0*258 <move\+0x78> ee ?0d ?f5 ?10 ? *	cfmv64lr	mvdx13, ?pc
0*25c <move\+0x7c> be ?04 ?95 ?10 ? *	cfmv64lrlt	mvdx4, ?r9
0*260 <move\+0x80> 9e ?00 ?a5 ?10 ? *	cfmv64lrls	mvdx0, ?sl
0*264 <move\+0x84> ee ?0a ?45 ?10 ? *	cfmv64lr	mvdx10, ?r4
0*268 <move\+0x88> 4e ?0e ?b5 ?10 ? *	cfmv64lrmi	mvdx14, ?fp
0*26c <move\+0x8c> 8e ?17 ?25 ?10 ? *	cfmvr64lhi	r2, ?mvdx7
0*270 <move\+0x90> 2e ?1c ?c5 ?10 ? *	cfmvr64lcs	ip, ?mvdx12
0*274 <move\+0x94> 6e ?10 ?35 ?10 ? *	cfmvr64lvs	r3, ?mvdx0
0*278 <move\+0x98> 7e ?1e ?d5 ?10 ? *	cfmvr64lvc	sp, ?mvdx14
0*27c <move\+0x9c> 3e ?1a ?e5 ?10 ? *	cfmvr64lcc	lr, ?mvdx10
0*280 <move\+0xa0> 1e ?08 ?25 ?30 ? *	cfmv64hrne	mvdx8, ?r2
0*284 <move\+0xa4> de ?06 ?c5 ?30 ? *	cfmv64hrle	mvdx6, ?ip
0*288 <move\+0xa8> 4e ?02 ?35 ?30 ? *	cfmv64hrmi	mvdx2, ?r3
0*28c <move\+0xac> 0e ?05 ?d5 ?30 ? *	cfmv64hreq	mvdx5, ?sp
0*290 <move\+0xb0> ae ?09 ?e5 ?30 ? *	cfmv64hrge	mvdx9, ?lr
0*294 <move\+0xb4> ee ?18 ?b5 ?30 ? *	cfmvr64h	fp, ?mvdx8
0*298 <move\+0xb8> de ?12 ?55 ?30 ? *	cfmvr64hle	r5, ?mvdx2
0*29c <move\+0xbc> 1e ?16 ?65 ?30 ? *	cfmvr64hne	r6, ?mvdx6
0*2a0 <move\+0xc0> be ?17 ?05 ?30 ? *	cfmvr64hlt	r0, ?mvdx7
0*2a4 <move\+0xc4> 5e ?13 ?75 ?30 ? *	cfmvr64hpl	r7, ?mvdx3
0*2a8 <move\+0xc8> ce ?21 ?14 ?40 ? *	cfmval32gt	mvax1, ?mvfx1
0*2ac <move\+0xcc> 8e ?2d ?34 ?40 ? *	cfmval32hi	mvax3, ?mvfx13
0*2b0 <move\+0xd0> 6e ?24 ?34 ?40 ? *	cfmval32vs	mvax3, ?mvfx4
0*2b4 <move\+0xd4> 2e ?20 ?14 ?40 ? *	cfmval32cs	mvax1, ?mvfx0
0*2b8 <move\+0xd8> 5e ?2a ?34 ?40 ? *	cfmval32pl	mvax3, ?mvfx10
0*2bc <move\+0xdc> 9e ?11 ?44 ?40 ? *	cfmv32alls	mvfx4, ?mvax1
0*2c0 <move\+0xe0> 3e ?13 ?84 ?40 ? *	cfmv32alcc	mvfx8, ?mvax3
0*2c4 <move\+0xe4> 7e ?13 ?24 ?40 ? *	cfmv32alvc	mvfx2, ?mvax3
0*2c8 <move\+0xe8> ce ?11 ?64 ?40 ? *	cfmv32algt	mvfx6, ?mvax1
0*2cc <move\+0xec> 0e ?13 ?74 ?40 ? *	cfmv32aleq	mvfx7, ?mvax3
0*2d0 <move\+0xf0> ee ?2c ?24 ?60 ? *	cfmvam32	mvax2, ?mvfx12
0*2d4 <move\+0xf4> ae ?28 ?34 ?60 ? *	cfmvam32ge	mvax3, ?mvfx8
0*2d8 <move\+0xf8> ee ?26 ?24 ?60 ? *	cfmvam32	mvax2, ?mvfx6
0*2dc <move\+0xfc> be ?22 ?24 ?60 ? *	cfmvam32lt	mvax2, ?mvfx2
0*2e0 <move\+0x100> 9e ?25 ?04 ?60 ? *	cfmvam32ls	mvax0, ?mvfx5
0*2e4 <move\+0x104> ee ?12 ?a4 ?60 ? *	cfmv32am	mvfx10, ?mvax2
0*2e8 <move\+0x108> 4e ?13 ?e4 ?60 ? *	cfmv32ammi	mvfx14, ?mvax3
0*2ec <move\+0x10c> 8e ?12 ?d4 ?60 ? *	cfmv32amhi	mvfx13, ?mvax2
0*2f0 <move\+0x110> 2e ?12 ?14 ?60 ? *	cfmv32amcs	mvfx1, ?mvax2
0*2f4 <move\+0x114> 6e ?10 ?b4 ?60 ? *	cfmv32amvs	mvfx11, ?mvax0
0*2f8 <move\+0x118> 7e ?2e ?34 ?80 ? *	cfmvah32vc	mvax3, ?mvfx14
0*2fc <move\+0x11c> 3e ?2a ?04 ?80 ? *	cfmvah32cc	mvax0, ?mvfx10
0*300 <move\+0x120> 1e ?2f ?14 ?80 ? *	cfmvah32ne	mvax1, ?mvfx15
0*304 <move\+0x124> de ?2b ?04 ?80 ? *	cfmvah32le	mvax0, ?mvfx11
0*308 <move\+0x128> 4e ?29 ?04 ?80 ? *	cfmvah32mi	mvax0, ?mvfx9
0*30c <move\+0x12c> 0e ?13 ?54 ?80 ? *	cfmv32aheq	mvfx5, ?mvax3
0*310 <move\+0x130> ae ?10 ?94 ?80 ? *	cfmv32ahge	mvfx9, ?mvax0
0*314 <move\+0x134> ee ?11 ?34 ?80 ? *	cfmv32ah	mvfx3, ?mvax1
0*318 <move\+0x138> de ?10 ?74 ?80 ? *	cfmv32ahle	mvfx7, ?mvax0
0*31c <move\+0x13c> 1e ?10 ?c4 ?80 ? *	cfmv32ahne	mvfx12, ?mvax0
0*320 <move\+0x140> be ?27 ?04 ?a0 ? *	cfmva32lt	mvax0, ?mvfx7
0*324 <move\+0x144> 5e ?23 ?24 ?a0 ? *	cfmva32pl	mvax2, ?mvfx3
0*328 <move\+0x148> ce ?21 ?14 ?a0 ? *	cfmva32gt	mvax1, ?mvfx1
0*32c <move\+0x14c> 8e ?2d ?34 ?a0 ? *	cfmva32hi	mvax3, ?mvfx13
0*330 <move\+0x150> 6e ?24 ?34 ?a0 ? *	cfmva32vs	mvax3, ?mvfx4
0*334 <move\+0x154> 2e ?10 ?94 ?a0 ? *	cfmv32acs	mvfx9, ?mvax0
0*338 <move\+0x158> 5e ?12 ?f4 ?a0 ? *	cfmv32apl	mvfx15, ?mvax2
0*33c <move\+0x15c> 9e ?11 ?44 ?a0 ? *	cfmv32als	mvfx4, ?mvax1
0*340 <move\+0x160> 3e ?13 ?84 ?a0 ? *	cfmv32acc	mvfx8, ?mvax3
0*344 <move\+0x164> 7e ?13 ?24 ?a0 ? *	cfmv32avc	mvfx2, ?mvax3
0*348 <move\+0x168> ce ?2b ?04 ?c0 ? *	cfmva64gt	mvax0, ?mvdx11
0*34c <move\+0x16c> 0e ?25 ?14 ?c0 ? *	cfmva64eq	mvax1, ?mvdx5
0*350 <move\+0x170> ee ?2c ?24 ?c0 ? *	cfmva64	mvax2, ?mvdx12
0*354 <move\+0x174> ae ?28 ?34 ?c0 ? *	cfmva64ge	mvax3, ?mvdx8
0*358 <move\+0x178> ee ?26 ?24 ?c0 ? *	cfmva64	mvax2, ?mvdx6
0*35c <move\+0x17c> be ?10 ?44 ?c0 ? *	cfmv64alt	mvdx4, ?mvax0
0*360 <move\+0x180> 9e ?11 ?04 ?c0 ? *	cfmv64als	mvdx0, ?mvax1
0*364 <move\+0x184> ee ?12 ?a4 ?c0 ? *	cfmv64a	mvdx10, ?mvax2
0*368 <move\+0x188> 4e ?13 ?e4 ?c0 ? *	cfmv64ami	mvdx14, ?mvax3
0*36c <move\+0x18c> 8e ?12 ?d4 ?c0 ? *	cfmv64ahi	mvdx13, ?mvax2
0*370 <move\+0x190> 2e ?20 ?c4 ?e0 ? *	cfmvsc32cs	dspsc, ?mvdx12
0*374 <move\+0x194> 6e ?20 ?04 ?e0 ? *	cfmvsc32vs	dspsc, ?mvdx0
0*378 <move\+0x198> 7e ?20 ?e4 ?e0 ? *	cfmvsc32vc	dspsc, ?mvdx14
0*37c <move\+0x19c> 3e ?20 ?a4 ?e0 ? *	cfmvsc32cc	dspsc, ?mvdx10
0*380 <move\+0x1a0> 1e ?20 ?f4 ?e0 ? *	cfmvsc32ne	dspsc, ?mvdx15
0*384 <move\+0x1a4> de ?10 ?64 ?e0 ? *	cfmv32scle	mvdx6, ?dspsc
0*388 <move\+0x1a8> 4e ?10 ?24 ?e0 ? *	cfmv32scmi	mvdx2, ?dspsc
0*38c <move\+0x1ac> 0e ?10 ?54 ?e0 ? *	cfmv32sceq	mvdx5, ?dspsc
0*390 <move\+0x1b0> ae ?10 ?94 ?e0 ? *	cfmv32scge	mvdx9, ?dspsc
0*394 <move\+0x1b4> ee ?10 ?34 ?e0 ? *	cfmv32sc	mvdx3, ?dspsc
0*398 <move\+0x1b8> de ?02 ?74 ?00 ? *	cfcpysle	mvf7, ?mvf2
0*39c <move\+0x1bc> 1e ?06 ?c4 ?00 ? *	cfcpysne	mvf12, ?mvf6
0*3a0 <move\+0x1c0> be ?07 ?04 ?00 ? *	cfcpyslt	mvf0, ?mvf7
0*3a4 <move\+0x1c4> 5e ?03 ?e4 ?00 ? *	cfcpyspl	mvf14, ?mvf3
0*3a8 <move\+0x1c8> ce ?01 ?a4 ?00 ? *	cfcpysgt	mvf10, ?mvf1
0*3ac <move\+0x1cc> 8e ?0d ?f4 ?20 ? *	cfcpydhi	mvd15, ?mvd13
0*3b0 <move\+0x1d0> 6e ?04 ?b4 ?20 ? *	cfcpydvs	mvd11, ?mvd4
0*3b4 <move\+0x1d4> 2e ?00 ?94 ?20 ? *	cfcpydcs	mvd9, ?mvd0
0*3b8 <move\+0x1d8> 5e ?0a ?f4 ?20 ? *	cfcpydpl	mvd15, ?mvd10
0*3bc <move\+0x1dc> 9e ?0e ?44 ?20 ? *	cfcpydls	mvd4, ?mvd14
# conv:
0*3c0 <conv> 3e ?0d ?84 ?60 ? *	cfcvtsdcc	mvd8, ?mvf13
0*3c4 <conv\+0x4> 7e ?01 ?24 ?60 ? *	cfcvtsdvc	mvd2, ?mvf1
0*3c8 <conv\+0x8> ce ?0b ?64 ?60 ? *	cfcvtsdgt	mvd6, ?mvf11
0*3cc <conv\+0xc> 0e ?05 ?74 ?60 ? *	cfcvtsdeq	mvd7, ?mvf5
0*3d0 <conv\+0x10> ee ?0c ?34 ?60 ? *	cfcvtsd	mvd3, ?mvf12
0*3d4 <conv\+0x14> ae ?08 ?14 ?40 ? *	cfcvtdsge	mvf1, ?mvd8
0*3d8 <conv\+0x18> ee ?06 ?d4 ?40 ? *	cfcvtds	mvf13, ?mvd6
0*3dc <conv\+0x1c> be ?02 ?44 ?40 ? *	cfcvtdslt	mvf4, ?mvd2
0*3e0 <conv\+0x20> 9e ?05 ?04 ?40 ? *	cfcvtdsls	mvf0, ?mvd5
0*3e4 <conv\+0x24> ee ?09 ?a4 ?40 ? *	cfcvtds	mvf10, ?mvd9
0*3e8 <conv\+0x28> 4e ?03 ?e4 ?80 ? *	cfcvt32smi	mvf14, ?mvfx3
0*3ec <conv\+0x2c> 8e ?07 ?d4 ?80 ? *	cfcvt32shi	mvf13, ?mvfx7
0*3f0 <conv\+0x30> 2e ?0c ?14 ?80 ? *	cfcvt32scs	mvf1, ?mvfx12
0*3f4 <conv\+0x34> 6e ?00 ?b4 ?80 ? *	cfcvt32svs	mvf11, ?mvfx0
0*3f8 <conv\+0x38> 7e ?0e ?54 ?80 ? *	cfcvt32svc	mvf5, ?mvfx14
0*3fc <conv\+0x3c> 3e ?0a ?c4 ?a0 ? *	cfcvt32dcc	mvd12, ?mvfx10
0*400 <conv\+0x40> 1e ?0f ?84 ?a0 ? *	cfcvt32dne	mvd8, ?mvfx15
0*404 <conv\+0x44> de ?0b ?64 ?a0 ? *	cfcvt32dle	mvd6, ?mvfx11
0*408 <conv\+0x48> 4e ?09 ?24 ?a0 ? *	cfcvt32dmi	mvd2, ?mvfx9
0*40c <conv\+0x4c> 0e ?0f ?54 ?a0 ? *	cfcvt32deq	mvd5, ?mvfx15
0*410 <conv\+0x50> ae ?04 ?94 ?c0 ? *	cfcvt64sge	mvf9, ?mvdx4
0*414 <conv\+0x54> ee ?08 ?34 ?c0 ? *	cfcvt64s	mvf3, ?mvdx8
0*418 <conv\+0x58> de ?02 ?74 ?c0 ? *	cfcvt64sle	mvf7, ?mvdx2
0*41c <conv\+0x5c> 1e ?06 ?c4 ?c0 ? *	cfcvt64sne	mvf12, ?mvdx6
0*420 <conv\+0x60> be ?07 ?04 ?c0 ? *	cfcvt64slt	mvf0, ?mvdx7
0*424 <conv\+0x64> 5e ?03 ?e4 ?e0 ? *	cfcvt64dpl	mvd14, ?mvdx3
0*428 <conv\+0x68> ce ?01 ?a4 ?e0 ? *	cfcvt64dgt	mvd10, ?mvdx1
0*42c <conv\+0x6c> 8e ?0d ?f4 ?e0 ? *	cfcvt64dhi	mvd15, ?mvdx13
0*430 <conv\+0x70> 6e ?04 ?b4 ?e0 ? *	cfcvt64dvs	mvd11, ?mvdx4
0*434 <conv\+0x74> 2e ?00 ?94 ?e0 ? *	cfcvt64dcs	mvd9, ?mvdx0
0*438 <conv\+0x78> 5e ?1a ?f5 ?80 ? *	cfcvts32pl	mvfx15, ?mvf10
0*43c <conv\+0x7c> 9e ?1e ?45 ?80 ? *	cfcvts32ls	mvfx4, ?mvf14
0*440 <conv\+0x80> 3e ?1d ?85 ?80 ? *	cfcvts32cc	mvfx8, ?mvf13
0*444 <conv\+0x84> 7e ?11 ?25 ?80 ? *	cfcvts32vc	mvfx2, ?mvf1
0*448 <conv\+0x88> ce ?1b ?65 ?80 ? *	cfcvts32gt	mvfx6, ?mvf11
0*44c <conv\+0x8c> 0e ?15 ?75 ?a0 ? *	cfcvtd32eq	mvfx7, ?mvd5
0*450 <conv\+0x90> ee ?1c ?35 ?a0 ? *	cfcvtd32	mvfx3, ?mvd12
0*454 <conv\+0x94> ae ?18 ?15 ?a0 ? *	cfcvtd32ge	mvfx1, ?mvd8
0*458 <conv\+0x98> ee ?16 ?d5 ?a0 ? *	cfcvtd32	mvfx13, ?mvd6
0*45c <conv\+0x9c> be ?12 ?45 ?a0 ? *	cfcvtd32lt	mvfx4, ?mvd2
0*460 <conv\+0xa0> 9e ?15 ?05 ?c0 ? *	cftruncs32ls	mvfx0, ?mvf5
0*464 <conv\+0xa4> ee ?19 ?a5 ?c0 ? *	cftruncs32	mvfx10, ?mvf9
0*468 <conv\+0xa8> 4e ?13 ?e5 ?c0 ? *	cftruncs32mi	mvfx14, ?mvf3
0*46c <conv\+0xac> 8e ?17 ?d5 ?c0 ? *	cftruncs32hi	mvfx13, ?mvf7
0*470 <conv\+0xb0> 2e ?1c ?15 ?c0 ? *	cftruncs32cs	mvfx1, ?mvf12
0*474 <conv\+0xb4> 6e ?10 ?b5 ?e0 ? *	cftruncd32vs	mvfx11, ?mvd0
0*478 <conv\+0xb8> 7e ?1e ?55 ?e0 ? *	cftruncd32vc	mvfx5, ?mvd14
0*47c <conv\+0xbc> 3e ?1a ?c5 ?e0 ? *	cftruncd32cc	mvfx12, ?mvd10
0*480 <conv\+0xc0> 1e ?1f ?85 ?e0 ? *	cftruncd32ne	mvfx8, ?mvd15
0*484 <conv\+0xc4> de ?1b ?65 ?e0 ? *	cftruncd32le	mvfx6, ?mvd11
# shift:
0*488 <shift> 4e ?02 ?05 ?59 ? *	cfrshl32mi	mvfx2, ?mvfx9, ?r0
0*48c <shift\+0x4> ee ?0a ?e5 ?59 ? *	cfrshl32	mvfx10, ?mvfx9, ?lr
0*490 <shift\+0x8> 3e ?08 ?55 ?5d ? *	cfrshl32cc	mvfx8, ?mvfx13, ?r5
0*494 <shift\+0xc> 1e ?0c ?35 ?56 ? *	cfrshl32ne	mvfx12, ?mvfx6, ?r3
0*498 <shift\+0x10> 7e ?05 ?45 ?5e ? *	cfrshl32vc	mvfx5, ?mvfx14, ?r4
0*49c <shift\+0x14> ae ?01 ?25 ?78 ? *	cfrshl64ge	mvdx1, ?mvdx8, ?r2
0*4a0 <shift\+0x18> 6e ?0b ?95 ?74 ? *	cfrshl64vs	mvdx11, ?mvdx4, ?r9
0*4a4 <shift\+0x1c> 0e ?05 ?75 ?7f ? *	cfrshl64eq	mvdx5, ?mvdx15, ?r7
0*4a8 <shift\+0x20> 4e ?0e ?85 ?73 ? *	cfrshl64mi	mvdx14, ?mvdx3, ?r8
0*4ac <shift\+0x24> 7e ?02 ?65 ?71 ? *	cfrshl64vc	mvdx2, ?mvdx1, ?r6
0*4b0 <shift\+0x28> be ?07 ?05 ?80 ? *	cfsh32lt	mvfx0, ?mvfx7, ?#-64
0*4b4 <shift\+0x2c> 3e ?0a ?c5 ?cc ? *	cfsh32cc	mvfx12, ?mvfx10, ?#-20
0*4b8 <shift\+0x30> ee ?06 ?d5 ?48 ? *	cfsh32	mvfx13, ?mvfx6, ?#40
0*4bc <shift\+0x34> 2e ?00 ?95 ?ef ? *	cfsh32cs	mvfx9, ?mvfx0, ?#-1
0*4c0 <shift\+0x38> ae ?04 ?95 ?28 ? *	cfsh32ge	mvfx9, ?mvfx4, ?#24
0*4c4 <shift\+0x3c> 8e ?27 ?d5 ?41 ? *	cfsh64hi	mvdx13, ?mvdx7, ?#33
0*4c8 <shift\+0x40> ce ?2b ?65 ?00 ? *	cfsh64gt	mvdx6, ?mvdx11, ?#0
0*4cc <shift\+0x44> 5e ?23 ?e5 ?40 ? *	cfsh64pl	mvdx14, ?mvdx3, ?#32
0*4d0 <shift\+0x48> 1e ?2f ?85 ?c1 ? *	cfsh64ne	mvdx8, ?mvdx15, ?#-31
0*4d4 <shift\+0x4c> be ?22 ?45 ?01 ? *	cfsh64lt	mvdx4, ?mvdx2, ?#1
# comp:
0*4d8 <comp> 5e ?1a ?d4 ?99 ? *	cfcmpspl	sp, ?mvf10, ?mvf9
0*4dc <comp\+0x4> ee ?18 ?b4 ?9d ? *	cfcmps	fp, ?mvf8, ?mvf13
0*4e0 <comp\+0x8> 2e ?1c ?c4 ?96 ? *	cfcmpscs	ip, ?mvf12, ?mvf6
0*4e4 <comp\+0xc> 0e ?15 ?a4 ?9e ? *	cfcmpseq	sl, ?mvf5, ?mvf14
0*4e8 <comp\+0x10> ce ?11 ?14 ?98 ? *	cfcmpsgt	r1, ?mvf1, ?mvf8
0*4ec <comp\+0x14> de ?1b ?f4 ?b4 ? *	cfcmpdle	pc, ?mvd11, ?mvd4
0*4f0 <comp\+0x18> 9e ?15 ?04 ?bf ? *	cfcmpdls	r0, ?mvd5, ?mvd15
0*4f4 <comp\+0x1c> 9e ?1e ?e4 ?b3 ? *	cfcmpdls	lr, ?mvd14, ?mvd3
0*4f8 <comp\+0x20> de ?12 ?54 ?b1 ? *	cfcmpdle	r5, ?mvd2, ?mvd1
0*4fc <comp\+0x24> 6e ?10 ?34 ?b7 ? *	cfcmpdvs	r3, ?mvd0, ?mvd7
0*500 <comp\+0x28> ee ?1c ?45 ?9a ? *	cfcmp32	r4, ?mvfx12, ?mvfx10
0*504 <comp\+0x2c> 8e ?1d ?25 ?96 ? *	cfcmp32hi	r2, ?mvfx13, ?mvfx6
0*508 <comp\+0x30> 4e ?19 ?95 ?90 ? *	cfcmp32mi	r9, ?mvfx9, ?mvfx0
0*50c <comp\+0x34> ee ?19 ?75 ?94 ? *	cfcmp32	r7, ?mvfx9, ?mvfx4
0*510 <comp\+0x38> 3e ?1d ?85 ?97 ? *	cfcmp32cc	r8, ?mvfx13, ?mvfx7
0*514 <comp\+0x3c> 1e ?16 ?65 ?bb ? *	cfcmp64ne	r6, ?mvdx6, ?mvdx11
0*518 <comp\+0x40> 7e ?1e ?d5 ?b3 ? *	cfcmp64vc	sp, ?mvdx14, ?mvdx3
0*51c <comp\+0x44> ae ?18 ?b5 ?bf ? *	cfcmp64ge	fp, ?mvdx8, ?mvdx15
0*520 <comp\+0x48> 6e ?14 ?c5 ?b2 ? *	cfcmp64vs	ip, ?mvdx4, ?mvdx2
0*524 <comp\+0x4c> 0e ?1f ?a5 ?ba ? *	cfcmp64eq	sl, ?mvdx15, ?mvdx10
# fp_arith:
0*528 <fp_arith> 4e ?33 ?e4 ?00 ? *	cfabssmi	mvf14, ?mvf3
0*52c <fp_arith\+0x4> 8e ?37 ?d4 ?00 ? *	cfabsshi	mvf13, ?mvf7
0*530 <fp_arith\+0x8> 2e ?3c ?14 ?00 ? *	cfabsscs	mvf1, ?mvf12
0*534 <fp_arith\+0xc> 6e ?30 ?b4 ?00 ? *	cfabssvs	mvf11, ?mvf0
0*538 <fp_arith\+0x10> 7e ?3e ?54 ?00 ? *	cfabssvc	mvf5, ?mvf14
0*53c <fp_arith\+0x14> 3e ?3a ?c4 ?20 ? *	cfabsdcc	mvd12, ?mvd10
0*540 <fp_arith\+0x18> 1e ?3f ?84 ?20 ? *	cfabsdne	mvd8, ?mvd15
0*544 <fp_arith\+0x1c> de ?3b ?64 ?20 ? *	cfabsdle	mvd6, ?mvd11
0*548 <fp_arith\+0x20> 4e ?39 ?24 ?20 ? *	cfabsdmi	mvd2, ?mvd9
0*54c <fp_arith\+0x24> 0e ?3f ?54 ?20 ? *	cfabsdeq	mvd5, ?mvd15
0*550 <fp_arith\+0x28> ae ?34 ?94 ?40 ? *	cfnegsge	mvf9, ?mvf4
0*554 <fp_arith\+0x2c> ee ?38 ?34 ?40 ? *	cfnegs	mvf3, ?mvf8
0*558 <fp_arith\+0x30> de ?32 ?74 ?40 ? *	cfnegsle	mvf7, ?mvf2
0*55c <fp_arith\+0x34> 1e ?36 ?c4 ?40 ? *	cfnegsne	mvf12, ?mvf6
0*560 <fp_arith\+0x38> be ?37 ?04 ?40 ? *	cfnegslt	mvf0, ?mvf7
0*564 <fp_arith\+0x3c> 5e ?33 ?e4 ?60 ? *	cfnegdpl	mvd14, ?mvd3
0*568 <fp_arith\+0x40> ce ?31 ?a4 ?60 ? *	cfnegdgt	mvd10, ?mvd1
0*56c <fp_arith\+0x44> 8e ?3d ?f4 ?60 ? *	cfnegdhi	mvd15, ?mvd13
0*570 <fp_arith\+0x48> 6e ?34 ?b4 ?60 ? *	cfnegdvs	mvd11, ?mvd4
0*574 <fp_arith\+0x4c> 2e ?30 ?94 ?60 ? *	cfnegdcs	mvd9, ?mvd0
0*578 <fp_arith\+0x50> 5e ?3a ?f4 ?89 ? *	cfaddspl	mvf15, ?mvf10, ?mvf9
0*57c <fp_arith\+0x54> ee ?38 ?34 ?8d ? *	cfadds	mvf3, ?mvf8, ?mvf13
0*580 <fp_arith\+0x58> 2e ?3c ?14 ?86 ? *	cfaddscs	mvf1, ?mvf12, ?mvf6
0*584 <fp_arith\+0x5c> 0e ?35 ?74 ?8e ? *	cfaddseq	mvf7, ?mvf5, ?mvf14
0*588 <fp_arith\+0x60> ce ?31 ?a4 ?88 ? *	cfaddsgt	mvf10, ?mvf1, ?mvf8
0*58c <fp_arith\+0x64> de ?3b ?64 ?a4 ? *	cfadddle	mvd6, ?mvd11, ?mvd4
0*590 <fp_arith\+0x68> 9e ?35 ?04 ?af ? *	cfadddls	mvd0, ?mvd5, ?mvd15
0*594 <fp_arith\+0x6c> 9e ?3e ?44 ?a3 ? *	cfadddls	mvd4, ?mvd14, ?mvd3
0*598 <fp_arith\+0x70> de ?32 ?74 ?a1 ? *	cfadddle	mvd7, ?mvd2, ?mvd1
0*59c <fp_arith\+0x74> 6e ?30 ?b4 ?a7 ? *	cfadddvs	mvd11, ?mvd0, ?mvd7
0*5a0 <fp_arith\+0x78> ee ?3c ?34 ?ca ? *	cfsubs	mvf3, ?mvf12, ?mvf10
0*5a4 <fp_arith\+0x7c> 8e ?3d ?f4 ?c6 ? *	cfsubshi	mvf15, ?mvf13, ?mvf6
0*5a8 <fp_arith\+0x80> 4e ?39 ?24 ?c0 ? *	cfsubsmi	mvf2, ?mvf9, ?mvf0
0*5ac <fp_arith\+0x84> ee ?39 ?a4 ?c4 ? *	cfsubs	mvf10, ?mvf9, ?mvf4
0*5b0 <fp_arith\+0x88> 3e ?3d ?84 ?c7 ? *	cfsubscc	mvf8, ?mvf13, ?mvf7
0*5b4 <fp_arith\+0x8c> 1e ?36 ?c4 ?eb ? *	cfsubdne	mvd12, ?mvd6, ?mvd11
0*5b8 <fp_arith\+0x90> 7e ?3e ?54 ?e3 ? *	cfsubdvc	mvd5, ?mvd14, ?mvd3
0*5bc <fp_arith\+0x94> ae ?38 ?14 ?ef ? *	cfsubdge	mvd1, ?mvd8, ?mvd15
0*5c0 <fp_arith\+0x98> 6e ?34 ?b4 ?e2 ? *	cfsubdvs	mvd11, ?mvd4, ?mvd2
0*5c4 <fp_arith\+0x9c> 0e ?3f ?54 ?ea ? *	cfsubdeq	mvd5, ?mvd15, ?mvd10
0*5c8 <fp_arith\+0xa0> 4e ?13 ?e4 ?08 ? *	cfmulsmi	mvf14, ?mvf3, ?mvf8
0*5cc <fp_arith\+0xa4> 7e ?11 ?24 ?0c ? *	cfmulsvc	mvf2, ?mvf1, ?mvf12
0*5d0 <fp_arith\+0xa8> be ?17 ?04 ?05 ? *	cfmulslt	mvf0, ?mvf7, ?mvf5
0*5d4 <fp_arith\+0xac> 3e ?1a ?c4 ?01 ? *	cfmulscc	mvf12, ?mvf10, ?mvf1
0*5d8 <fp_arith\+0xb0> ee ?16 ?d4 ?0b ? *	cfmuls	mvf13, ?mvf6, ?mvf11
0*5dc <fp_arith\+0xb4> 2e ?10 ?94 ?25 ? *	cfmuldcs	mvd9, ?mvd0, ?mvd5
0*5e0 <fp_arith\+0xb8> ae ?14 ?94 ?2e ? *	cfmuldge	mvd9, ?mvd4, ?mvd14
0*5e4 <fp_arith\+0xbc> 8e ?17 ?d4 ?22 ? *	cfmuldhi	mvd13, ?mvd7, ?mvd2
0*5e8 <fp_arith\+0xc0> ce ?1b ?64 ?20 ? *	cfmuldgt	mvd6, ?mvd11, ?mvd0
0*5ec <fp_arith\+0xc4> 5e ?13 ?e4 ?2c ? *	cfmuldpl	mvd14, ?mvd3, ?mvd12
# int_arith:
0*5f0 <int_arith> 1e ?3f ?85 ?00 ? *	cfabs32ne	mvfx8, ?mvfx15
0*5f4 <int_arith\+0x4> de ?3b ?65 ?00 ? *	cfabs32le	mvfx6, ?mvfx11
0*5f8 <int_arith\+0x8> 4e ?39 ?25 ?00 ? *	cfabs32mi	mvfx2, ?mvfx9
0*5fc <int_arith\+0xc> 0e ?3f ?55 ?00 ? *	cfabs32eq	mvfx5, ?mvfx15
0*600 <int_arith\+0x10> ae ?34 ?95 ?00 ? *	cfabs32ge	mvfx9, ?mvfx4
0*604 <int_arith\+0x14> ee ?38 ?35 ?20 ? *	cfabs64	mvdx3, ?mvdx8
0*608 <int_arith\+0x18> de ?32 ?75 ?20 ? *	cfabs64le	mvdx7, ?mvdx2
0*60c <int_arith\+0x1c> 1e ?36 ?c5 ?20 ? *	cfabs64ne	mvdx12, ?mvdx6
0*610 <int_arith\+0x20> be ?37 ?05 ?20 ? *	cfabs64lt	mvdx0, ?mvdx7
0*614 <int_arith\+0x24> 5e ?33 ?e5 ?20 ? *	cfabs64pl	mvdx14, ?mvdx3
0*618 <int_arith\+0x28> ce ?31 ?a5 ?40 ? *	cfneg32gt	mvfx10, ?mvfx1
0*61c <int_arith\+0x2c> 8e ?3d ?f5 ?40 ? *	cfneg32hi	mvfx15, ?mvfx13
0*620 <int_arith\+0x30> 6e ?34 ?b5 ?40 ? *	cfneg32vs	mvfx11, ?mvfx4
0*624 <int_arith\+0x34> 2e ?30 ?95 ?40 ? *	cfneg32cs	mvfx9, ?mvfx0
0*628 <int_arith\+0x38> 5e ?3a ?f5 ?40 ? *	cfneg32pl	mvfx15, ?mvfx10
0*62c <int_arith\+0x3c> 9e ?3e ?45 ?60 ? *	cfneg64ls	mvdx4, ?mvdx14
0*630 <int_arith\+0x40> 3e ?3d ?85 ?60 ? *	cfneg64cc	mvdx8, ?mvdx13
0*634 <int_arith\+0x44> 7e ?31 ?25 ?60 ? *	cfneg64vc	mvdx2, ?mvdx1
0*638 <int_arith\+0x48> ce ?3b ?65 ?60 ? *	cfneg64gt	mvdx6, ?mvdx11
0*63c <int_arith\+0x4c> 0e ?35 ?75 ?60 ? *	cfneg64eq	mvdx7, ?mvdx5
0*640 <int_arith\+0x50> ee ?3c ?35 ?8a ? *	cfadd32	mvfx3, ?mvfx12, ?mvfx10
0*644 <int_arith\+0x54> 8e ?3d ?f5 ?86 ? *	cfadd32hi	mvfx15, ?mvfx13, ?mvfx6
0*648 <int_arith\+0x58> 4e ?39 ?25 ?80 ? *	cfadd32mi	mvfx2, ?mvfx9, ?mvfx0
0*64c <int_arith\+0x5c> ee ?39 ?a5 ?84 ? *	cfadd32	mvfx10, ?mvfx9, ?mvfx4
0*650 <int_arith\+0x60> 3e ?3d ?85 ?87 ? *	cfadd32cc	mvfx8, ?mvfx13, ?mvfx7
0*654 <int_arith\+0x64> 1e ?36 ?c5 ?ab ? *	cfadd64ne	mvdx12, ?mvdx6, ?mvdx11
0*658 <int_arith\+0x68> 7e ?3e ?55 ?a3 ? *	cfadd64vc	mvdx5, ?mvdx14, ?mvdx3
0*65c <int_arith\+0x6c> ae ?38 ?15 ?af ? *	cfadd64ge	mvdx1, ?mvdx8, ?mvdx15
0*660 <int_arith\+0x70> 6e ?34 ?b5 ?a2 ? *	cfadd64vs	mvdx11, ?mvdx4, ?mvdx2
0*664 <int_arith\+0x74> 0e ?3f ?55 ?aa ? *	cfadd64eq	mvdx5, ?mvdx15, ?mvdx10
0*668 <int_arith\+0x78> 4e ?33 ?e5 ?c8 ? *	cfsub32mi	mvfx14, ?mvfx3, ?mvfx8
0*66c <int_arith\+0x7c> 7e ?31 ?25 ?cc ? *	cfsub32vc	mvfx2, ?mvfx1, ?mvfx12
0*670 <int_arith\+0x80> be ?37 ?05 ?c5 ? *	cfsub32lt	mvfx0, ?mvfx7, ?mvfx5
0*674 <int_arith\+0x84> 3e ?3a ?c5 ?c1 ? *	cfsub32cc	mvfx12, ?mvfx10, ?mvfx1
0*678 <int_arith\+0x88> ee ?36 ?d5 ?cb ? *	cfsub32	mvfx13, ?mvfx6, ?mvfx11
0*67c <int_arith\+0x8c> 2e ?30 ?95 ?e5 ? *	cfsub64cs	mvdx9, ?mvdx0, ?mvdx5
0*680 <int_arith\+0x90> ae ?34 ?95 ?ee ? *	cfsub64ge	mvdx9, ?mvdx4, ?mvdx14
0*684 <int_arith\+0x94> 8e ?37 ?d5 ?e2 ? *	cfsub64hi	mvdx13, ?mvdx7, ?mvdx2
0*688 <int_arith\+0x98> ce ?3b ?65 ?e0 ? *	cfsub64gt	mvdx6, ?mvdx11, ?mvdx0
0*68c <int_arith\+0x9c> 5e ?33 ?e5 ?ec ? *	cfsub64pl	mvdx14, ?mvdx3, ?mvdx12
0*690 <int_arith\+0xa0> 1e ?1f ?85 ?0d ? *	cfmul32ne	mvfx8, ?mvfx15, ?mvfx13
0*694 <int_arith\+0xa4> be ?12 ?45 ?09 ? *	cfmul32lt	mvfx4, ?mvfx2, ?mvfx9
0*698 <int_arith\+0xa8> 5e ?1a ?f5 ?09 ? *	cfmul32pl	mvfx15, ?mvfx10, ?mvfx9
0*69c <int_arith\+0xac> ee ?18 ?35 ?0d ? *	cfmul32	mvfx3, ?mvfx8, ?mvfx13
0*6a0 <int_arith\+0xb0> 2e ?1c ?15 ?06 ? *	cfmul32cs	mvfx1, ?mvfx12, ?mvfx6
0*6a4 <int_arith\+0xb4> 0e ?15 ?75 ?2e ? *	cfmul64eq	mvdx7, ?mvdx5, ?mvdx14
0*6a8 <int_arith\+0xb8> ce ?11 ?a5 ?28 ? *	cfmul64gt	mvdx10, ?mvdx1, ?mvdx8
0*6ac <int_arith\+0xbc> de ?1b ?65 ?24 ? *	cfmul64le	mvdx6, ?mvdx11, ?mvdx4
0*6b0 <int_arith\+0xc0> 9e ?15 ?05 ?2f ? *	cfmul64ls	mvdx0, ?mvdx5, ?mvdx15
0*6b4 <int_arith\+0xc4> 9e ?1e ?45 ?23 ? *	cfmul64ls	mvdx4, ?mvdx14, ?mvdx3
0*6b8 <int_arith\+0xc8> de ?12 ?75 ?41 ? *	cfmac32le	mvfx7, ?mvfx2, ?mvfx1
0*6bc <int_arith\+0xcc> 6e ?10 ?b5 ?47 ? *	cfmac32vs	mvfx11, ?mvfx0, ?mvfx7
0*6c0 <int_arith\+0xd0> ee ?1c ?35 ?4a ? *	cfmac32	mvfx3, ?mvfx12, ?mvfx10
0*6c4 <int_arith\+0xd4> 8e ?1d ?f5 ?46 ? *	cfmac32hi	mvfx15, ?mvfx13, ?mvfx6
0*6c8 <int_arith\+0xd8> 4e ?19 ?25 ?40 ? *	cfmac32mi	mvfx2, ?mvfx9, ?mvfx0
0*6cc <int_arith\+0xdc> ee ?19 ?a5 ?64 ? *	cfmsc32	mvfx10, ?mvfx9, ?mvfx4
0*6d0 <int_arith\+0xe0> 3e ?1d ?85 ?67 ? *	cfmsc32cc	mvfx8, ?mvfx13, ?mvfx7
0*6d4 <int_arith\+0xe4> 1e ?16 ?c5 ?6b ? *	cfmsc32ne	mvfx12, ?mvfx6, ?mvfx11
0*6d8 <int_arith\+0xe8> 7e ?1e ?55 ?63 ? *	cfmsc32vc	mvfx5, ?mvfx14, ?mvfx3
0*6dc <int_arith\+0xec> ae ?18 ?15 ?6f ? *	cfmsc32ge	mvfx1, ?mvfx8, ?mvfx15
# acc_arith:
0*6e0 <acc_arith> 6e ?02 ?46 ?69 ? *	cfmadd32vs	mvax3, ?mvfx4, ?mvfx2, ?mvfx9
0*6e4 <acc_arith\+0x4> 0e ?0a ?f6 ?29 ? *	cfmadd32eq	mvax1, ?mvfx15, ?mvfx10, ?mvfx9
0*6e8 <acc_arith\+0x8> 4e ?08 ?36 ?2d ? *	cfmadd32mi	mvax1, ?mvfx3, ?mvfx8, ?mvfx13
0*6ec <acc_arith\+0xc> 7e ?0c ?16 ?06 ? *	cfmadd32vc	mvax0, ?mvfx1, ?mvfx12, ?mvfx6
0*6f0 <acc_arith\+0x10> be ?05 ?76 ?0e ? *	cfmadd32lt	mvax0, ?mvfx7, ?mvfx5, ?mvfx14
0*6f4 <acc_arith\+0x14> 3e ?11 ?a6 ?08 ? *	cfmsub32cc	mvax0, ?mvfx10, ?mvfx1, ?mvfx8
0*6f8 <acc_arith\+0x18> ee ?1b ?66 ?44 ? *	cfmsub32	mvax2, ?mvfx6, ?mvfx11, ?mvfx4
0*6fc <acc_arith\+0x1c> 2e ?15 ?06 ?2f ? *	cfmsub32cs	mvax1, ?mvfx0, ?mvfx5, ?mvfx15
0*700 <acc_arith\+0x20> ae ?1e ?46 ?43 ? *	cfmsub32ge	mvax2, ?mvfx4, ?mvfx14, ?mvfx3
0*704 <acc_arith\+0x24> 8e ?12 ?76 ?61 ? *	cfmsub32hi	mvax3, ?mvfx7, ?mvfx2, ?mvfx1
0*708 <acc_arith\+0x28> ce ?20 ?16 ?07 ? *	cfmadda32gt	mvax0, ?mvax1, ?mvfx0, ?mvfx7
0*70c <acc_arith\+0x2c> 5e ?2c ?26 ?4a ? *	cfmadda32pl	mvax2, ?mvax2, ?mvfx12, ?mvfx10
0*710 <acc_arith\+0x30> 1e ?2d ?36 ?26 ? *	cfmadda32ne	mvax1, ?mvax3, ?mvfx13, ?mvfx6
0*714 <acc_arith\+0x34> be ?29 ?06 ?40 ? *	cfmadda32lt	mvax2, ?mvax0, ?mvfx9, ?mvfx0
0*718 <acc_arith\+0x38> 5e ?29 ?26 ?64 ? *	cfmadda32pl	mvax3, ?mvax2, ?mvfx9, ?mvfx4
0*71c <acc_arith\+0x3c> ee ?3d ?16 ?67 ? *	cfmsuba32	mvax3, ?mvax1, ?mvfx13, ?mvfx7
0*720 <acc_arith\+0x40> 2e ?36 ?26 ?6b ? *	cfmsuba32cs	mvax3, ?mvax2, ?mvfx6, ?mvfx11
0*724 <acc_arith\+0x44> 0e ?3e ?36 ?23 ? *	cfmsuba32eq	mvax1, ?mvax3, ?mvfx14, ?mvfx3
0*728 <acc_arith\+0x48> ce ?38 ?36 ?2f ? *	cfmsuba32gt	mvax1, ?mvax3, ?mvfx8, ?mvfx15
0*72c <acc_arith\+0x4c> de ?34 ?36 ?02 ? *	cfmsuba32le	mvax0, ?mvax3, ?mvfx4, ?mvfx2
