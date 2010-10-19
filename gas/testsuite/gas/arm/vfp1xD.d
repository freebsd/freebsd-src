#objdump: -dr --prefix-addresses --show-raw-insn
#name: VFP Single-precision instructions
#as: -mfpu=vfpxd

# Test the ARM VFP Single Precision instructions

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> eef1fa10 	fmstat
0+004 <[^>]*> eeb40ac0 	fcmpes	s0, s0
0+008 <[^>]*> eeb50ac0 	fcmpezs	s0
0+00c <[^>]*> eeb40a40 	fcmps	s0, s0
0+010 <[^>]*> eeb50a40 	fcmpzs	s0
0+014 <[^>]*> eeb00ac0 	fabss	s0, s0
0+018 <[^>]*> eeb00a40 	fcpys	s0, s0
0+01c <[^>]*> eeb10a40 	fnegs	s0, s0
0+020 <[^>]*> eeb10ac0 	fsqrts	s0, s0
0+024 <[^>]*> ee300a00 	fadds	s0, s0, s0
0+028 <[^>]*> ee800a00 	fdivs	s0, s0, s0
0+02c <[^>]*> ee000a00 	fmacs	s0, s0, s0
0+030 <[^>]*> ee100a00 	fmscs	s0, s0, s0
0+034 <[^>]*> ee200a00 	fmuls	s0, s0, s0
0+038 <[^>]*> ee000a40 	fnmacs	s0, s0, s0
0+03c <[^>]*> ee100a40 	fnmscs	s0, s0, s0
0+040 <[^>]*> ee200a40 	fnmuls	s0, s0, s0
0+044 <[^>]*> ee300a40 	fsubs	s0, s0, s0
0+048 <[^>]*> ed900a00 	flds	s0, \[r0\]
0+04c <[^>]*> ed800a00 	fsts	s0, \[r0\]
0+050 <[^>]*> ec900a01 	fldmias	r0, {s0}
0+054 <[^>]*> ec900a01 	fldmias	r0, {s0}
0+058 <[^>]*> ecb00a01 	fldmias	r0!, {s0}
0+05c <[^>]*> ecb00a01 	fldmias	r0!, {s0}
0+060 <[^>]*> ed300a01 	fldmdbs	r0!, {s0}
0+064 <[^>]*> ed300a01 	fldmdbs	r0!, {s0}
0+068 <[^>]*> ec900b03 	fldmiax	r0, {d0}
0+06c <[^>]*> ec900b03 	fldmiax	r0, {d0}
0+070 <[^>]*> ecb00b03 	fldmiax	r0!, {d0}
0+074 <[^>]*> ecb00b03 	fldmiax	r0!, {d0}
0+078 <[^>]*> ed300b03 	fldmdbx	r0!, {d0}
0+07c <[^>]*> ed300b03 	fldmdbx	r0!, {d0}
0+080 <[^>]*> ec800a01 	fstmias	r0, {s0}
0+084 <[^>]*> ec800a01 	fstmias	r0, {s0}
0+088 <[^>]*> eca00a01 	fstmias	r0!, {s0}
0+08c <[^>]*> eca00a01 	fstmias	r0!, {s0}
0+090 <[^>]*> ed200a01 	fstmdbs	r0!, {s0}
0+094 <[^>]*> ed200a01 	fstmdbs	r0!, {s0}
0+098 <[^>]*> ec800b03 	fstmiax	r0, {d0}
0+09c <[^>]*> ec800b03 	fstmiax	r0, {d0}
0+0a0 <[^>]*> eca00b03 	fstmiax	r0!, {d0}
0+0a4 <[^>]*> eca00b03 	fstmiax	r0!, {d0}
0+0a8 <[^>]*> ed200b03 	fstmdbx	r0!, {d0}
0+0ac <[^>]*> ed200b03 	fstmdbx	r0!, {d0}
0+0b0 <[^>]*> eeb80ac0 	fsitos	s0, s0
0+0b4 <[^>]*> eeb80a40 	fuitos	s0, s0
0+0b8 <[^>]*> eebd0a40 	ftosis	s0, s0
0+0bc <[^>]*> eebd0ac0 	ftosizs	s0, s0
0+0c0 <[^>]*> eebc0a40 	ftouis	s0, s0
0+0c4 <[^>]*> eebc0ac0 	ftouizs	s0, s0
0+0c8 <[^>]*> ee100a10 	fmrs	r0, s0
0+0cc <[^>]*> eef00a10 	fmrx	r0, fpsid
0+0d0 <[^>]*> eef10a10 	fmrx	r0, fpscr
0+0d4 <[^>]*> eef80a10 	fmrx	r0, fpexc
0+0d8 <[^>]*> ee000a10 	fmsr	s0, r0
0+0dc <[^>]*> eee00a10 	fmxr	fpsid, r0
0+0e0 <[^>]*> eee10a10 	fmxr	fpscr, r0
0+0e4 <[^>]*> eee80a10 	fmxr	fpexc, r0
0+0e8 <[^>]*> eef50a40 	fcmpzs	s1
0+0ec <[^>]*> eeb51a40 	fcmpzs	s2
0+0f0 <[^>]*> eef5fa40 	fcmpzs	s31
0+0f4 <[^>]*> eeb40a60 	fcmps	s0, s1
0+0f8 <[^>]*> eeb40a41 	fcmps	s0, s2
0+0fc <[^>]*> eeb40a6f 	fcmps	s0, s31
0+100 <[^>]*> eef40a40 	fcmps	s1, s0
0+104 <[^>]*> eeb41a40 	fcmps	s2, s0
0+108 <[^>]*> eef4fa40 	fcmps	s31, s0
0+10c <[^>]*> eef4aa46 	fcmps	s21, s12
0+110 <[^>]*> eeb10a60 	fnegs	s0, s1
0+114 <[^>]*> eeb10a41 	fnegs	s0, s2
0+118 <[^>]*> eeb10a6f 	fnegs	s0, s31
0+11c <[^>]*> eef10a40 	fnegs	s1, s0
0+120 <[^>]*> eeb11a40 	fnegs	s2, s0
0+124 <[^>]*> eef1fa40 	fnegs	s31, s0
0+128 <[^>]*> eeb16a6a 	fnegs	s12, s21
0+12c <[^>]*> ee300a20 	fadds	s0, s0, s1
0+130 <[^>]*> ee300a01 	fadds	s0, s0, s2
0+134 <[^>]*> ee300a2f 	fadds	s0, s0, s31
0+138 <[^>]*> ee300a80 	fadds	s0, s1, s0
0+13c <[^>]*> ee310a00 	fadds	s0, s2, s0
0+140 <[^>]*> ee3f0a80 	fadds	s0, s31, s0
0+144 <[^>]*> ee700a00 	fadds	s1, s0, s0
0+148 <[^>]*> ee301a00 	fadds	s2, s0, s0
0+14c <[^>]*> ee70fa00 	fadds	s31, s0, s0
0+150 <[^>]*> ee3a6aa2 	fadds	s12, s21, s5
0+154 <[^>]*> eeb80ae0 	fsitos	s0, s1
0+158 <[^>]*> eeb80ac1 	fsitos	s0, s2
0+15c <[^>]*> eeb80aef 	fsitos	s0, s31
0+160 <[^>]*> eef80ac0 	fsitos	s1, s0
0+164 <[^>]*> eeb81ac0 	fsitos	s2, s0
0+168 <[^>]*> eef8fac0 	fsitos	s31, s0
0+16c <[^>]*> eebd0a60 	ftosis	s0, s1
0+170 <[^>]*> eebd0a41 	ftosis	s0, s2
0+174 <[^>]*> eebd0a6f 	ftosis	s0, s31
0+178 <[^>]*> eefd0a40 	ftosis	s1, s0
0+17c <[^>]*> eebd1a40 	ftosis	s2, s0
0+180 <[^>]*> eefdfa40 	ftosis	s31, s0
0+184 <[^>]*> ee001a10 	fmsr	s0, r1
0+188 <[^>]*> ee007a10 	fmsr	s0, r7
0+18c <[^>]*> ee00ea10 	fmsr	s0, lr
0+190 <[^>]*> ee000a90 	fmsr	s1, r0
0+194 <[^>]*> ee010a10 	fmsr	s2, r0
0+198 <[^>]*> ee0f0a90 	fmsr	s31, r0
0+19c <[^>]*> ee0a7a90 	fmsr	s21, r7
0+1a0 <[^>]*> eee01a10 	fmxr	fpsid, r1
0+1a4 <[^>]*> eee0ea10 	fmxr	fpsid, lr
0+1a8 <[^>]*> ee100a90 	fmrs	r0, s1
0+1ac <[^>]*> ee110a10 	fmrs	r0, s2
0+1b0 <[^>]*> ee1f0a90 	fmrs	r0, s31
0+1b4 <[^>]*> ee101a10 	fmrs	r1, s0
0+1b8 <[^>]*> ee107a10 	fmrs	r7, s0
0+1bc <[^>]*> ee10ea10 	fmrs	lr, s0
0+1c0 <[^>]*> ee159a90 	fmrs	r9, s11
0+1c4 <[^>]*> eef01a10 	fmrx	r1, fpsid
0+1c8 <[^>]*> eef0ea10 	fmrx	lr, fpsid
0+1cc <[^>]*> ed910a00 	flds	s0, \[r1\]
0+1d0 <[^>]*> ed9e0a00 	flds	s0, \[lr\]
0+1d4 <[^>]*> ed900a00 	flds	s0, \[r0\]
0+1d8 <[^>]*> ed900aff 	flds	s0, \[r0, #1020\]
0+1dc <[^>]*> ed100aff 	flds	s0, \[r0, #-1020\]
0+1e0 <[^>]*> edd00a00 	flds	s1, \[r0\]
0+1e4 <[^>]*> ed901a00 	flds	s2, \[r0\]
0+1e8 <[^>]*> edd0fa00 	flds	s31, \[r0\]
0+1ec <[^>]*> edccaac9 	fsts	s21, \[ip, #804\]
0+1f0 <[^>]*> ecd00a01 	fldmias	r0, {s1}
0+1f4 <[^>]*> ec901a01 	fldmias	r0, {s2}
0+1f8 <[^>]*> ecd0fa01 	fldmias	r0, {s31}
0+1fc <[^>]*> ec900a02 	fldmias	r0, {s0-s1}
0+200 <[^>]*> ec900a03 	fldmias	r0, {s0-s2}
0+204 <[^>]*> ec900a20 	fldmias	r0, {s0-s31}
0+208 <[^>]*> ecd00a1f 	fldmias	r0, {s1-s31}
0+20c <[^>]*> ec901a1e 	fldmias	r0, {s2-s31}
0+210 <[^>]*> ec90fa02 	fldmias	r0, {s30-s31}
0+214 <[^>]*> ec910a01 	fldmias	r1, {s0}
0+218 <[^>]*> ec9e0a01 	fldmias	lr, {s0}
0+21c <[^>]*> ec801b03 	fstmiax	r0, {d1}
0+220 <[^>]*> ec802b03 	fstmiax	r0, {d2}
0+224 <[^>]*> ec80fb03 	fstmiax	r0, {d15}
0+228 <[^>]*> ec800b05 	fstmiax	r0, {d0-d1}
0+22c <[^>]*> ec800b07 	fstmiax	r0, {d0-d2}
0+230 <[^>]*> ec800b21 	fstmiax	r0, {d0-d15}
0+234 <[^>]*> ec801b1f 	fstmiax	r0, {d1-d15}
0+238 <[^>]*> ec802b1d 	fstmiax	r0, {d2-d15}
0+23c <[^>]*> ec80eb05 	fstmiax	r0, {d14-d15}
0+240 <[^>]*> ec810b03 	fstmiax	r1, {d0}
0+244 <[^>]*> ec8e0b03 	fstmiax	lr, {d0}
0+248 <[^>]*> eeb50a40 	fcmpzs	s0
0+24c <[^>]*> eef50a40 	fcmpzs	s1
0+250 <[^>]*> eeb51a40 	fcmpzs	s2
0+254 <[^>]*> eef51a40 	fcmpzs	s3
0+258 <[^>]*> eeb52a40 	fcmpzs	s4
0+25c <[^>]*> eef52a40 	fcmpzs	s5
0+260 <[^>]*> eeb53a40 	fcmpzs	s6
0+264 <[^>]*> eef53a40 	fcmpzs	s7
0+268 <[^>]*> eeb54a40 	fcmpzs	s8
0+26c <[^>]*> eef54a40 	fcmpzs	s9
0+270 <[^>]*> eeb55a40 	fcmpzs	s10
0+274 <[^>]*> eef55a40 	fcmpzs	s11
0+278 <[^>]*> eeb56a40 	fcmpzs	s12
0+27c <[^>]*> eef56a40 	fcmpzs	s13
0+280 <[^>]*> eeb57a40 	fcmpzs	s14
0+284 <[^>]*> eef57a40 	fcmpzs	s15
0+288 <[^>]*> eeb58a40 	fcmpzs	s16
0+28c <[^>]*> eef58a40 	fcmpzs	s17
0+290 <[^>]*> eeb59a40 	fcmpzs	s18
0+294 <[^>]*> eef59a40 	fcmpzs	s19
0+298 <[^>]*> eeb5aa40 	fcmpzs	s20
0+29c <[^>]*> eef5aa40 	fcmpzs	s21
0+2a0 <[^>]*> eeb5ba40 	fcmpzs	s22
0+2a4 <[^>]*> eef5ba40 	fcmpzs	s23
0+2a8 <[^>]*> eeb5ca40 	fcmpzs	s24
0+2ac <[^>]*> eef5ca40 	fcmpzs	s25
0+2b0 <[^>]*> eeb5da40 	fcmpzs	s26
0+2b4 <[^>]*> eef5da40 	fcmpzs	s27
0+2b8 <[^>]*> eeb5ea40 	fcmpzs	s28
0+2bc <[^>]*> eef5ea40 	fcmpzs	s29
0+2c0 <[^>]*> eeb5fa40 	fcmpzs	s30
0+2c4 <[^>]*> eef5fa40 	fcmpzs	s31
0+2c8 <[^>]*> 0ef1fa10 	fmstateq
0+2cc <[^>]*> 0ef41ae3 	fcmpeseq	s3, s7
0+2d0 <[^>]*> 0ef52ac0 	fcmpezseq	s5
0+2d4 <[^>]*> 0ef40a41 	fcmpseq	s1, s2
0+2d8 <[^>]*> 0ef50a40 	fcmpzseq	s1
0+2dc <[^>]*> 0ef00ae1 	fabsseq	s1, s3
0+2e0 <[^>]*> 0ef0fa69 	fcpyseq	s31, s19
0+2e4 <[^>]*> 0eb1aa44 	fnegseq	s20, s8
0+2e8 <[^>]*> 0ef12ae3 	fsqrtseq	s5, s7
0+2ec <[^>]*> 0e323a82 	faddseq	s6, s5, s4
0+2f0 <[^>]*> 0ec11a20 	fdivseq	s3, s2, s1
0+2f4 <[^>]*> 0e4ffa2e 	fmacseq	s31, s30, s29
0+2f8 <[^>]*> 0e1dea8d 	fmscseq	s28, s27, s26
0+2fc <[^>]*> 0e6cca2b 	fmulseq	s25, s24, s23
0+300 <[^>]*> 0e0abaca 	fnmacseq	s22, s21, s20
0+304 <[^>]*> 0e599a68 	fnmscseq	s19, s18, s17
0+308 <[^>]*> 0e278ac7 	fnmulseq	s16, s15, s14
0+30c <[^>]*> 0e766a65 	fsubseq	s13, s12, s11
0+310 <[^>]*> 0d985a00 	fldseq	s10, \[r8\]
0+314 <[^>]*> 0dc74a00 	fstseq	s9, \[r7\]
0+318 <[^>]*> 0c914a01 	fldmiaseq	r1, {s8}
0+31c <[^>]*> 0cd23a01 	fldmiaseq	r2, {s7}
0+320 <[^>]*> 0cb33a01 	fldmiaseq	r3!, {s6}
0+324 <[^>]*> 0cf42a01 	fldmiaseq	r4!, {s5}
0+328 <[^>]*> 0d352a01 	fldmdbseq	r5!, {s4}
0+32c <[^>]*> 0d761a01 	fldmdbseq	r6!, {s3}
0+330 <[^>]*> 0c971b03 	fldmiaxeq	r7, {d1}
0+334 <[^>]*> 0c982b03 	fldmiaxeq	r8, {d2}
0+338 <[^>]*> 0cb93b03 	fldmiaxeq	r9!, {d3}
0+33c <[^>]*> 0cba4b03 	fldmiaxeq	sl!, {d4}
0+340 <[^>]*> 0d3b5b03 	fldmdbxeq	fp!, {d5}
0+344 <[^>]*> 0d3c6b03 	fldmdbxeq	ip!, {d6}
0+348 <[^>]*> 0c8d1a01 	fstmiaseq	sp, {s2}
0+34c <[^>]*> 0cce0a01 	fstmiaseq	lr, {s1}
0+350 <[^>]*> 0ce1fa01 	fstmiaseq	r1!, {s31}
0+354 <[^>]*> 0ca2fa01 	fstmiaseq	r2!, {s30}
0+358 <[^>]*> 0d63ea01 	fstmdbseq	r3!, {s29}
0+35c <[^>]*> 0d24ea01 	fstmdbseq	r4!, {s28}
0+360 <[^>]*> 0c857b03 	fstmiaxeq	r5, {d7}
0+364 <[^>]*> 0c868b03 	fstmiaxeq	r6, {d8}
0+368 <[^>]*> 0ca79b03 	fstmiaxeq	r7!, {d9}
0+36c <[^>]*> 0ca8ab03 	fstmiaxeq	r8!, {d10}
0+370 <[^>]*> 0d29bb03 	fstmdbxeq	r9!, {d11}
0+374 <[^>]*> 0d2acb03 	fstmdbxeq	sl!, {d12}
0+378 <[^>]*> 0ef8dac3 	fsitoseq	s27, s6
0+37c <[^>]*> 0efdca62 	ftosiseq	s25, s5
0+380 <[^>]*> 0efdbac2 	ftosizseq	s23, s4
0+384 <[^>]*> 0efcaa61 	ftouiseq	s21, s3
0+388 <[^>]*> 0efc9ac1 	ftouizseq	s19, s2
0+38c <[^>]*> 0ef88a60 	fuitoseq	s17, s1
0+390 <[^>]*> 0e11ba90 	fmrseq	fp, s3
0+394 <[^>]*> 0ef09a10 	fmrxeq	r9, fpsid
0+398 <[^>]*> 0e019a90 	fmsreq	s3, r9
0+39c <[^>]*> 0ee08a10 	fmxreq	fpsid, r8
