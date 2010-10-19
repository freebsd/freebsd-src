#objdump: -dr --prefix-addresses --show-raw-insn
#name: Thumb-2 VFP Single-precision instructions
#as: -mfpu=vfpxd

# Test the ARM VFP Single Precision instructions

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> eef1 fa10 	fmstat
0+004 <[^>]*> eeb4 0ac0 	fcmpes	s0, s0
0+008 <[^>]*> eeb5 0ac0 	fcmpezs	s0
0+00c <[^>]*> eeb4 0a40 	fcmps	s0, s0
0+010 <[^>]*> eeb5 0a40 	fcmpzs	s0
0+014 <[^>]*> eeb0 0ac0 	fabss	s0, s0
0+018 <[^>]*> eeb0 0a40 	fcpys	s0, s0
0+01c <[^>]*> eeb1 0a40 	fnegs	s0, s0
0+020 <[^>]*> eeb1 0ac0 	fsqrts	s0, s0
0+024 <[^>]*> ee30 0a00 	fadds	s0, s0, s0
0+028 <[^>]*> ee80 0a00 	fdivs	s0, s0, s0
0+02c <[^>]*> ee00 0a00 	fmacs	s0, s0, s0
0+030 <[^>]*> ee10 0a00 	fmscs	s0, s0, s0
0+034 <[^>]*> ee20 0a00 	fmuls	s0, s0, s0
0+038 <[^>]*> ee00 0a40 	fnmacs	s0, s0, s0
0+03c <[^>]*> ee10 0a40 	fnmscs	s0, s0, s0
0+040 <[^>]*> ee20 0a40 	fnmuls	s0, s0, s0
0+044 <[^>]*> ee30 0a40 	fsubs	s0, s0, s0
0+048 <[^>]*> ed90 0a00 	flds	s0, \[r0\]
0+04c <[^>]*> ed80 0a00 	fsts	s0, \[r0\]
0+050 <[^>]*> ec90 0a01 	fldmias	r0, {s0}
0+054 <[^>]*> ec90 0a01 	fldmias	r0, {s0}
0+058 <[^>]*> ecb0 0a01 	fldmias	r0!, {s0}
0+05c <[^>]*> ecb0 0a01 	fldmias	r0!, {s0}
0+060 <[^>]*> ed30 0a01 	fldmdbs	r0!, {s0}
0+064 <[^>]*> ed30 0a01 	fldmdbs	r0!, {s0}
0+068 <[^>]*> ec90 0b03 	fldmiax	r0, {d0}
0+06c <[^>]*> ec90 0b03 	fldmiax	r0, {d0}
0+070 <[^>]*> ecb0 0b03 	fldmiax	r0!, {d0}
0+074 <[^>]*> ecb0 0b03 	fldmiax	r0!, {d0}
0+078 <[^>]*> ed30 0b03 	fldmdbx	r0!, {d0}
0+07c <[^>]*> ed30 0b03 	fldmdbx	r0!, {d0}
0+080 <[^>]*> ec80 0a01 	fstmias	r0, {s0}
0+084 <[^>]*> ec80 0a01 	fstmias	r0, {s0}
0+088 <[^>]*> eca0 0a01 	fstmias	r0!, {s0}
0+08c <[^>]*> eca0 0a01 	fstmias	r0!, {s0}
0+090 <[^>]*> ed20 0a01 	fstmdbs	r0!, {s0}
0+094 <[^>]*> ed20 0a01 	fstmdbs	r0!, {s0}
0+098 <[^>]*> ec80 0b03 	fstmiax	r0, {d0}
0+09c <[^>]*> ec80 0b03 	fstmiax	r0, {d0}
0+0a0 <[^>]*> eca0 0b03 	fstmiax	r0!, {d0}
0+0a4 <[^>]*> eca0 0b03 	fstmiax	r0!, {d0}
0+0a8 <[^>]*> ed20 0b03 	fstmdbx	r0!, {d0}
0+0ac <[^>]*> ed20 0b03 	fstmdbx	r0!, {d0}
0+0b0 <[^>]*> eeb8 0ac0 	fsitos	s0, s0
0+0b4 <[^>]*> eeb8 0a40 	fuitos	s0, s0
0+0b8 <[^>]*> eebd 0a40 	ftosis	s0, s0
0+0bc <[^>]*> eebd 0ac0 	ftosizs	s0, s0
0+0c0 <[^>]*> eebc 0a40 	ftouis	s0, s0
0+0c4 <[^>]*> eebc 0ac0 	ftouizs	s0, s0
0+0c8 <[^>]*> ee10 0a10 	fmrs	r0, s0
0+0cc <[^>]*> eef0 0a10 	fmrx	r0, fpsid
0+0d0 <[^>]*> eef1 0a10 	fmrx	r0, fpscr
0+0d4 <[^>]*> eef8 0a10 	fmrx	r0, fpexc
0+0d8 <[^>]*> ee00 0a10 	fmsr	s0, r0
0+0dc <[^>]*> eee0 0a10 	fmxr	fpsid, r0
0+0e0 <[^>]*> eee1 0a10 	fmxr	fpscr, r0
0+0e4 <[^>]*> eee8 0a10 	fmxr	fpexc, r0
0+0e8 <[^>]*> eef5 0a40 	fcmpzs	s1
0+0ec <[^>]*> eeb5 1a40 	fcmpzs	s2
0+0f0 <[^>]*> eef5 fa40 	fcmpzs	s31
0+0f4 <[^>]*> eeb4 0a60 	fcmps	s0, s1
0+0f8 <[^>]*> eeb4 0a41 	fcmps	s0, s2
0+0fc <[^>]*> eeb4 0a6f 	fcmps	s0, s31
0+100 <[^>]*> eef4 0a40 	fcmps	s1, s0
0+104 <[^>]*> eeb4 1a40 	fcmps	s2, s0
0+108 <[^>]*> eef4 fa40 	fcmps	s31, s0
0+10c <[^>]*> eef4 aa46 	fcmps	s21, s12
0+110 <[^>]*> eeb1 0a60 	fnegs	s0, s1
0+114 <[^>]*> eeb1 0a41 	fnegs	s0, s2
0+118 <[^>]*> eeb1 0a6f 	fnegs	s0, s31
0+11c <[^>]*> eef1 0a40 	fnegs	s1, s0
0+120 <[^>]*> eeb1 1a40 	fnegs	s2, s0
0+124 <[^>]*> eef1 fa40 	fnegs	s31, s0
0+128 <[^>]*> eeb1 6a6a 	fnegs	s12, s21
0+12c <[^>]*> ee30 0a20 	fadds	s0, s0, s1
0+130 <[^>]*> ee30 0a01 	fadds	s0, s0, s2
0+134 <[^>]*> ee30 0a2f 	fadds	s0, s0, s31
0+138 <[^>]*> ee30 0a80 	fadds	s0, s1, s0
0+13c <[^>]*> ee31 0a00 	fadds	s0, s2, s0
0+140 <[^>]*> ee3f 0a80 	fadds	s0, s31, s0
0+144 <[^>]*> ee70 0a00 	fadds	s1, s0, s0
0+148 <[^>]*> ee30 1a00 	fadds	s2, s0, s0
0+14c <[^>]*> ee70 fa00 	fadds	s31, s0, s0
0+150 <[^>]*> ee3a 6aa2 	fadds	s12, s21, s5
0+154 <[^>]*> eeb8 0ae0 	fsitos	s0, s1
0+158 <[^>]*> eeb8 0ac1 	fsitos	s0, s2
0+15c <[^>]*> eeb8 0aef 	fsitos	s0, s31
0+160 <[^>]*> eef8 0ac0 	fsitos	s1, s0
0+164 <[^>]*> eeb8 1ac0 	fsitos	s2, s0
0+168 <[^>]*> eef8 fac0 	fsitos	s31, s0
0+16c <[^>]*> eebd 0a60 	ftosis	s0, s1
0+170 <[^>]*> eebd 0a41 	ftosis	s0, s2
0+174 <[^>]*> eebd 0a6f 	ftosis	s0, s31
0+178 <[^>]*> eefd 0a40 	ftosis	s1, s0
0+17c <[^>]*> eebd 1a40 	ftosis	s2, s0
0+180 <[^>]*> eefd fa40 	ftosis	s31, s0
0+184 <[^>]*> ee00 1a10 	fmsr	s0, r1
0+188 <[^>]*> ee00 7a10 	fmsr	s0, r7
0+18c <[^>]*> ee00 ea10 	fmsr	s0, lr
0+190 <[^>]*> ee00 0a90 	fmsr	s1, r0
0+194 <[^>]*> ee01 0a10 	fmsr	s2, r0
0+198 <[^>]*> ee0f 0a90 	fmsr	s31, r0
0+19c <[^>]*> ee0a 7a90 	fmsr	s21, r7
0+1a0 <[^>]*> eee0 1a10 	fmxr	fpsid, r1
0+1a4 <[^>]*> eee0 ea10 	fmxr	fpsid, lr
0+1a8 <[^>]*> ee10 0a90 	fmrs	r0, s1
0+1ac <[^>]*> ee11 0a10 	fmrs	r0, s2
0+1b0 <[^>]*> ee1f 0a90 	fmrs	r0, s31
0+1b4 <[^>]*> ee10 1a10 	fmrs	r1, s0
0+1b8 <[^>]*> ee10 7a10 	fmrs	r7, s0
0+1bc <[^>]*> ee10 ea10 	fmrs	lr, s0
0+1c0 <[^>]*> ee15 9a90 	fmrs	r9, s11
0+1c4 <[^>]*> eef0 1a10 	fmrx	r1, fpsid
0+1c8 <[^>]*> eef0 ea10 	fmrx	lr, fpsid
0+1cc <[^>]*> ed91 0a00 	flds	s0, \[r1\]
0+1d0 <[^>]*> ed9e 0a00 	flds	s0, \[lr\]
0+1d4 <[^>]*> ed90 0a00 	flds	s0, \[r0\]
0+1d8 <[^>]*> ed90 0aff 	flds	s0, \[r0, #1020\]
0+1dc <[^>]*> ed10 0aff 	flds	s0, \[r0, #-1020\]
0+1e0 <[^>]*> edd0 0a00 	flds	s1, \[r0\]
0+1e4 <[^>]*> ed90 1a00 	flds	s2, \[r0\]
0+1e8 <[^>]*> edd0 fa00 	flds	s31, \[r0\]
0+1ec <[^>]*> edcc aac9 	fsts	s21, \[ip, #804\]
0+1f0 <[^>]*> ecd0 0a01 	fldmias	r0, {s1}
0+1f4 <[^>]*> ec90 1a01 	fldmias	r0, {s2}
0+1f8 <[^>]*> ecd0 fa01 	fldmias	r0, {s31}
0+1fc <[^>]*> ec90 0a02 	fldmias	r0, {s0-s1}
0+200 <[^>]*> ec90 0a03 	fldmias	r0, {s0-s2}
0+204 <[^>]*> ec90 0a20 	fldmias	r0, {s0-s31}
0+208 <[^>]*> ecd0 0a1f 	fldmias	r0, {s1-s31}
0+20c <[^>]*> ec90 1a1e 	fldmias	r0, {s2-s31}
0+210 <[^>]*> ec90 fa02 	fldmias	r0, {s30-s31}
0+214 <[^>]*> ec91 0a01 	fldmias	r1, {s0}
0+218 <[^>]*> ec9e 0a01 	fldmias	lr, {s0}
0+21c <[^>]*> ec80 1b03 	fstmiax	r0, {d1}
0+220 <[^>]*> ec80 2b03 	fstmiax	r0, {d2}
0+224 <[^>]*> ec80 fb03 	fstmiax	r0, {d15}
0+228 <[^>]*> ec80 0b05 	fstmiax	r0, {d0-d1}
0+22c <[^>]*> ec80 0b07 	fstmiax	r0, {d0-d2}
0+230 <[^>]*> ec80 0b21 	fstmiax	r0, {d0-d15}
0+234 <[^>]*> ec80 1b1f 	fstmiax	r0, {d1-d15}
0+238 <[^>]*> ec80 2b1d 	fstmiax	r0, {d2-d15}
0+23c <[^>]*> ec80 eb05 	fstmiax	r0, {d14-d15}
0+240 <[^>]*> ec81 0b03 	fstmiax	r1, {d0}
0+244 <[^>]*> ec8e 0b03 	fstmiax	lr, {d0}
0+248 <[^>]*> eeb5 0a40 	fcmpzs	s0
0+24c <[^>]*> eef5 0a40 	fcmpzs	s1
0+250 <[^>]*> eeb5 1a40 	fcmpzs	s2
0+254 <[^>]*> eef5 1a40 	fcmpzs	s3
0+258 <[^>]*> eeb5 2a40 	fcmpzs	s4
0+25c <[^>]*> eef5 2a40 	fcmpzs	s5
0+260 <[^>]*> eeb5 3a40 	fcmpzs	s6
0+264 <[^>]*> eef5 3a40 	fcmpzs	s7
0+268 <[^>]*> eeb5 4a40 	fcmpzs	s8
0+26c <[^>]*> eef5 4a40 	fcmpzs	s9
0+270 <[^>]*> eeb5 5a40 	fcmpzs	s10
0+274 <[^>]*> eef5 5a40 	fcmpzs	s11
0+278 <[^>]*> eeb5 6a40 	fcmpzs	s12
0+27c <[^>]*> eef5 6a40 	fcmpzs	s13
0+280 <[^>]*> eeb5 7a40 	fcmpzs	s14
0+284 <[^>]*> eef5 7a40 	fcmpzs	s15
0+288 <[^>]*> eeb5 8a40 	fcmpzs	s16
0+28c <[^>]*> eef5 8a40 	fcmpzs	s17
0+290 <[^>]*> eeb5 9a40 	fcmpzs	s18
0+294 <[^>]*> eef5 9a40 	fcmpzs	s19
0+298 <[^>]*> eeb5 aa40 	fcmpzs	s20
0+29c <[^>]*> eef5 aa40 	fcmpzs	s21
0+2a0 <[^>]*> eeb5 ba40 	fcmpzs	s22
0+2a4 <[^>]*> eef5 ba40 	fcmpzs	s23
0+2a8 <[^>]*> eeb5 ca40 	fcmpzs	s24
0+2ac <[^>]*> eef5 ca40 	fcmpzs	s25
0+2b0 <[^>]*> eeb5 da40 	fcmpzs	s26
0+2b4 <[^>]*> eef5 da40 	fcmpzs	s27
0+2b8 <[^>]*> eeb5 ea40 	fcmpzs	s28
0+2bc <[^>]*> eef5 ea40 	fcmpzs	s29
0+2c0 <[^>]*> eeb5 fa40 	fcmpzs	s30
0+2c4 <[^>]*> eef5 fa40 	fcmpzs	s31
# The "(eq|)" should be replaces by "eq" once the disassembler is fixed.
0+2c8 <[^>]*> bf01      	itttt	eq
0+2ca <[^>]*> eef1 fa10 	fmstat(eq|)
0+2ce <[^>]*> eef4 1ae3 	fcmpes(eq|)	s3, s7
0+2d2 <[^>]*> eef5 2ac0 	fcmpezs(eq|)	s5
0+2d6 <[^>]*> eef4 0a41 	fcmps(eq|)	s1, s2
0+2da <[^>]*> bf01      	itttt	eq
0+2dc <[^>]*> eef5 0a40 	fcmpzs(eq|)	s1
0+2e0 <[^>]*> eef0 0ae1 	fabss(eq|)	s1, s3
0+2e4 <[^>]*> eef0 fa69 	fcpys(eq|)	s31, s19
0+2e8 <[^>]*> eeb1 aa44 	fnegs(eq|)	s20, s8
0+2ec <[^>]*> bf01      	itttt	eq
0+2ee <[^>]*> eef1 2ae3 	fsqrts(eq|)	s5, s7
0+2f2 <[^>]*> ee32 3a82 	fadds(eq|)	s6, s5, s4
0+2f6 <[^>]*> eec1 1a20 	fdivs(eq|)	s3, s2, s1
0+2fa <[^>]*> ee4f fa2e 	fmacs(eq|)	s31, s30, s29
0+2fe <[^>]*> bf01      	itttt	eq
0+300 <[^>]*> ee1d ea8d 	fmscs(eq|)	s28, s27, s26
0+304 <[^>]*> ee6c ca2b 	fmuls(eq|)	s25, s24, s23
0+308 <[^>]*> ee0a baca 	fnmacs(eq|)	s22, s21, s20
0+30c <[^>]*> ee59 9a68 	fnmscs(eq|)	s19, s18, s17
0+310 <[^>]*> bf01      	itttt	eq
0+312 <[^>]*> ee27 8ac7 	fnmuls(eq|)	s16, s15, s14
0+316 <[^>]*> ee76 6a65 	fsubs(eq|)	s13, s12, s11
0+31a <[^>]*> ed98 5a00 	flds(eq|)	s10, \[r8\]
0+31e <[^>]*> edc7 4a00 	fsts(eq|)	s9, \[r7\]
0+322 <[^>]*> bf01      	itttt	eq
0+324 <[^>]*> ec91 4a01 	fldmias(eq|)	r1, {s8}
0+328 <[^>]*> ecd2 3a01 	fldmias(eq|)	r2, {s7}
0+32c <[^>]*> ecb3 3a01 	fldmias(eq|)	r3!, {s6}
0+330 <[^>]*> ecf4 2a01 	fldmias(eq|)	r4!, {s5}
0+334 <[^>]*> bf01      	itttt	eq
0+336 <[^>]*> ed35 2a01 	fldmdbs(eq|)	r5!, {s4}
0+33a <[^>]*> ed76 1a01 	fldmdbs(eq|)	r6!, {s3}
0+33e <[^>]*> ec97 1b03 	fldmiax(eq|)	r7, {d1}
0+342 <[^>]*> ec98 2b03 	fldmiax(eq|)	r8, {d2}
0+346 <[^>]*> bf01      	itttt	eq
0+348 <[^>]*> ecb9 3b03 	fldmiax(eq|)	r9!, {d3}
0+34c <[^>]*> ecba 4b03 	fldmiax(eq|)	sl!, {d4}
0+350 <[^>]*> ed3b 5b03 	fldmdbx(eq|)	fp!, {d5}
0+354 <[^>]*> ed3c 6b03 	fldmdbx(eq|)	ip!, {d6}
0+358 <[^>]*> bf01      	itttt	eq
0+35a <[^>]*> ec8d 1a01 	fstmias(eq|)	sp, {s2}
0+35e <[^>]*> ecce 0a01 	fstmias(eq|)	lr, {s1}
0+362 <[^>]*> ece1 fa01 	fstmias(eq|)	r1!, {s31}
0+366 <[^>]*> eca2 fa01 	fstmias(eq|)	r2!, {s30}
0+36a <[^>]*> bf01      	itttt	eq
0+36c <[^>]*> ed63 ea01 	fstmdbs(eq|)	r3!, {s29}
0+370 <[^>]*> ed24 ea01 	fstmdbs(eq|)	r4!, {s28}
0+374 <[^>]*> ec85 7b03 	fstmiax(eq|)	r5, {d7}
0+378 <[^>]*> ec86 8b03 	fstmiax(eq|)	r6, {d8}
0+37c <[^>]*> bf01      	itttt	eq
0+37e <[^>]*> eca7 9b03 	fstmiax(eq|)	r7!, {d9}
0+382 <[^>]*> eca8 ab03 	fstmiax(eq|)	r8!, {d10}
0+386 <[^>]*> ed29 bb03 	fstmdbx(eq|)	r9!, {d11}
0+38a <[^>]*> ed2a cb03 	fstmdbx(eq|)	sl!, {d12}
0+38e <[^>]*> bf01      	itttt	eq
0+390 <[^>]*> eef8 dac3 	fsitos(eq|)	s27, s6
0+394 <[^>]*> eefd ca62 	ftosis(eq|)	s25, s5
0+398 <[^>]*> eefd bac2 	ftosizs(eq|)	s23, s4
0+39c <[^>]*> eefc aa61 	ftouis(eq|)	s21, s3
0+3a0 <[^>]*> bf01      	itttt	eq
0+3a2 <[^>]*> eefc 9ac1 	ftouizs(eq|)	s19, s2
0+3a6 <[^>]*> eef8 8a60 	fuitos(eq|)	s17, s1
0+3aa <[^>]*> ee11 ba90 	fmrs(eq|)	fp, s3
0+3ae <[^>]*> eef0 9a10 	fmrx(eq|)	r9, fpsid
0+3b2 <[^>]*> bf04      	itt	eq
0+3b4 <[^>]*> ee01 9a90 	fmsr(eq|)	s3, r9
0+3b8 <[^>]*> eee0 8a10 	fmxr(eq|)	fpsid, r8
0+3bc <[^>]*> bf00      	nop
0+3be <[^>]*> bf00      	nop
