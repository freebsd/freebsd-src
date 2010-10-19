#name: ARM V6 instructions
#as: -march=armv6j
#objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> f102000f ?	cps	#15
0+004 <[^>]*> f10c00c0 ?	cpsid	if
0+008 <[^>]*> f10800c0 ?	cpsie	if
0+00c <[^>]*> e1942f9f ?	ldrex	r2, \[r4\]
0+010 <[^>]*> 11984f9f ?	ldrexne	r4, \[r8\]
0+014 <[^>]*> fc4570c3 ?	mcrr2	0, 12, r7, r5, cr3
0+018 <[^>]*> fc5570c3 ?	mrrc2	0, 12, r7, r5, cr3
0+01c <[^>]*> e6852018 ?	pkhbt	r2, r5, r8
0+020 <[^>]*> e6852198 ?	pkhbt	r2, r5, r8, LSL #3
0+024 <[^>]*> e6852198 ?	pkhbt	r2, r5, r8, LSL #3
0+028 <[^>]*> 06852198 ?	pkhbteq	r2, r5, r8, LSL #3
0+02c <[^>]*> e6882015 ?	pkhbt	r2, r8, r5
0+030 <[^>]*> e68521d8 ?	pkhtb	r2, r5, r8, ASR #3
0+034 <[^>]*> e68521d8 ?	pkhtb	r2, r5, r8, ASR #3
0+038 <[^>]*> 068521d8 ?	pkhtbeq	r2, r5, r8, ASR #3
0+03c <[^>]*> e6242f17 ?	qadd16	r2, r4, r7
0+040 <[^>]*> 16242f17 ?	qadd16ne	r2, r4, r7
0+044 <[^>]*> e6242f97 ?	qadd8	r2, r4, r7
0+048 <[^>]*> 16242f97 ?	qadd8ne	r2, r4, r7
0+04c <[^>]*> e6242f37 ?	qaddsubx	r2, r4, r7
0+050 <[^>]*> 16242f37 ?	qaddsubxne	r2, r4, r7
0+054 <[^>]*> e6242f77 ?	qsub16	r2, r4, r7
0+058 <[^>]*> 16242f77 ?	qsub16ne	r2, r4, r7
0+05c <[^>]*> e6242ff7 ?	qsub8	r2, r4, r7
0+060 <[^>]*> 16242ff7 ?	qsub8ne	r2, r4, r7
0+064 <[^>]*> e6242f57 ?	qsubaddx	r2, r4, r7
0+068 <[^>]*> e6242f57 ?	qsubaddx	r2, r4, r7
0+06c <[^>]*> e6bf2f34 ?	rev	r2, r4
0+070 <[^>]*> e6bf2fb4 ?	rev16	r2, r4
0+074 <[^>]*> 16bf3fb5 ?	rev16ne	r3, r5
0+078 <[^>]*> 16bf3f35 ?	revne	r3, r5
0+07c <[^>]*> e6ff2fb4 ?	revsh	r2, r4
0+080 <[^>]*> 16ff3fb5 ?	revshne	r3, r5
0+084 <[^>]*> f8120a00 ?	rfeda	r2
0+088 <[^>]*> f9320a00 ?	rfedb	r2!
0+08c <[^>]*> f8120a00 ?	rfeda	r2
0+090 <[^>]*> f9320a00 ?	rfedb	r2!
0+094 <[^>]*> f9b20a00 ?	rfeib	r2!
0+098 <[^>]*> f8920a00 ?	rfeia	r2
0+09c <[^>]*> f8920a00 ?	rfeia	r2
0+0a0 <[^>]*> f9b20a00 ?	rfeib	r2!
0+0a4 <[^>]*> e6142f17 ?	sadd16	r2, r4, r7
0+0a8 <[^>]*> 16142f17 ?	sadd16ne	r2, r4, r7
0+0ac <[^>]*> e6b42075 ?	sxtah	r2, r4, r5
0+0b0 <[^>]*> e6b42475 ?	sxtah	r2, r4, r5, ROR #8
0+0b4 <[^>]*> 16b42075 ?	sxtahne	r2, r4, r5
0+0b8 <[^>]*> 16b42475 ?	sxtahne	r2, r4, r5, ROR #8
0+0bc <[^>]*> e6142f97 ?	sadd8	r2, r4, r7
0+0c0 <[^>]*> 16142f97 ?	sadd8ne	r2, r4, r7
0+0c4 <[^>]*> e6842075 ?	sxtab16	r2, r4, r5
0+0c8 <[^>]*> e6842475 ?	sxtab16	r2, r4, r5, ROR #8
0+0cc <[^>]*> 16842075 ?	sxtab16ne	r2, r4, r5
0+0d0 <[^>]*> 16842475 ?	sxtab16ne	r2, r4, r5, ROR #8
0+0d4 <[^>]*> e6a42075 ?	sxtab	r2, r4, r5
0+0d8 <[^>]*> e6a42475 ?	sxtab	r2, r4, r5, ROR #8
0+0dc <[^>]*> 16a42075 ?	sxtabne	r2, r4, r5
0+0e0 <[^>]*> 16a42475 ?	sxtabne	r2, r4, r5, ROR #8
0+0e4 <[^>]*> e6142f37 ?	saddaddx	r2, r4, r7
0+0e8 <[^>]*> 16142f37 ?	saddaddxne	r2, r4, r7
0+0ec <[^>]*> e6821fb3 ?	sel	r1, r2, r3
0+0f0 <[^>]*> 16821fb3 ?	selne	r1, r2, r3
0+0f4 <[^>]*> f1010200 ?	setend	be
0+0f8 <[^>]*> f1010000 ?	setend	le
0+0fc <[^>]*> e6342f17 ?	shadd16	r2, r4, r7
0+100 <[^>]*> 16342f17 ?	shadd16ne	r2, r4, r7
0+104 <[^>]*> e6342f97 ?	shadd8	r2, r4, r7
0+108 <[^>]*> 16342f97 ?	shadd8ne	r2, r4, r7
0+10c <[^>]*> e6342f37 ?	shaddsubx	r2, r4, r7
0+110 <[^>]*> 16342f37 ?	shaddsubxne	r2, r4, r7
0+114 <[^>]*> e6342f77 ?	shsub16	r2, r4, r7
0+118 <[^>]*> 16342f77 ?	shsub16ne	r2, r4, r7
0+11c <[^>]*> e6342ff7 ?	shsub8	r2, r4, r7
0+120 <[^>]*> 16342ff7 ?	shsub8ne	r2, r4, r7
0+124 <[^>]*> e6342f57 ?	shsubaddx	r2, r4, r7
0+128 <[^>]*> 16342f57 ?	shsubaddxne	r2, r4, r7
0+12c <[^>]*> e7014312 ?	smlad	r1, r2, r3, r4
0+130 <[^>]*> d7014312 ?	smladle	r1, r2, r3, r4
0+134 <[^>]*> e7014332 ?	smladx	r1, r2, r3, r4
0+138 <[^>]*> d7014332 ?	smladxle	r1, r2, r3, r4
0+13c <[^>]*> e7421413 ?	smlald	r1, r2, r3, r4
0+140 <[^>]*> d7421413 ?	smlaldle	r1, r2, r3, r4
0+144 <[^>]*> e7421433 ?	smlaldx	r1, r2, r3, r4
0+148 <[^>]*> d7421433 ?	smlaldxle	r1, r2, r3, r4
0+14c <[^>]*> e7014352 ?	smlsd	r1, r2, r3, r4
0+150 <[^>]*> d7014352 ?	smlsdle	r1, r2, r3, r4
0+154 <[^>]*> e7014372 ?	smlsdx	r1, r2, r3, r4
0+158 <[^>]*> d7014372 ?	smlsdxle	r1, r2, r3, r4
0+15c <[^>]*> e7421453 ?	smlsld	r1, r2, r3, r4
0+160 <[^>]*> d7421453 ?	smlsldle	r1, r2, r3, r4
0+164 <[^>]*> e7421473 ?	smlsldx	r1, r2, r3, r4
0+168 <[^>]*> d7421473 ?	smlsldxle	r1, r2, r3, r4
0+16c <[^>]*> e7514312 ?	smmla	r1, r2, r3, r4
0+170 <[^>]*> d7514312 ?	smmlale	r1, r2, r3, r4
0+174 <[^>]*> e7514332 ?	smmlar	r1, r2, r3, r4
0+178 <[^>]*> d7514332 ?	smmlarle	r1, r2, r3, r4
0+17c <[^>]*> e75143d2 ?	smmls	r1, r2, r3, r4
0+180 <[^>]*> d75143d2 ?	smmlsle	r1, r2, r3, r4
0+184 <[^>]*> e75143f2 ?	smmlsr	r1, r2, r3, r4
0+188 <[^>]*> d75143f2 ?	smmlsrle	r1, r2, r3, r4
0+18c <[^>]*> e751f312 ?	smmul	r1, r2, r3
0+190 <[^>]*> d751f312 ?	smmulle	r1, r2, r3
0+194 <[^>]*> e751f332 ?	smmulr	r1, r2, r3
0+198 <[^>]*> d751f332 ?	smmulrle	r1, r2, r3
0+19c <[^>]*> e701f312 ?	smuad	r1, r2, r3
0+1a0 <[^>]*> d701f312 ?	smuadle	r1, r2, r3
0+1a4 <[^>]*> e701f332 ?	smuadx	r1, r2, r3
0+1a8 <[^>]*> d701f332 ?	smuadxle	r1, r2, r3
0+1ac <[^>]*> e701f352 ?	smusd	r1, r2, r3
0+1b0 <[^>]*> d701f352 ?	smusdle	r1, r2, r3
0+1b4 <[^>]*> e701f372 ?	smusdx	r1, r2, r3
0+1b8 <[^>]*> d701f372 ?	smusdxle	r1, r2, r3
0+1bc <[^>]*> f8cd0510 ?	srsia	#16
0+1c0 <[^>]*> f9ed0510 ?	srsib	#16!
0+1c4 <[^>]*> e6a01012 ?	ssat	r1, #1, r2
0+1c8 <[^>]*> e6a01152 ?	ssat	r1, #1, r2, ASR #2
0+1cc <[^>]*> e6a01112 ?	ssat	r1, #1, r2, LSL #2
0+1d0 <[^>]*> e6a01f31 ?	ssat16	r1, #1, r1
0+1d4 <[^>]*> d6a01f31 ?	ssat16le	r1, #1, r1
0+1d8 <[^>]*> e6142f77 ?	ssub16	r2, r4, r7
0+1dc <[^>]*> 16142f77 ?	ssub16ne	r2, r4, r7
0+1e0 <[^>]*> e6142ff7 ?	ssub8	r2, r4, r7
0+1e4 <[^>]*> 16142ff7 ?	ssub8ne	r2, r4, r7
0+1e8 <[^>]*> e6142f57 ?	ssubaddx	r2, r4, r7
0+1ec <[^>]*> 16142f57 ?	ssubaddxne	r2, r4, r7
0+1f0 <[^>]*> e1831f92 ?	strex	r1, r2, \[r3\]
0+1f4 <[^>]*> 11831f92 ?	strexne	r1, r2, \[r3\]
0+1f8 <[^>]*> e6bf2075 ?	sxth r2,r5
0+1fc <[^>]*> e6bf2475 ?	sxth r2,r5, ROR #8
0+200 <[^>]*> 16bf2075 ?	sxthne r2,r5
0+204 <[^>]*> 16bf2475 ?	sxthne r2,r5, ROR #8
0+208 <[^>]*> e68f2075 ?	sxtb16 r2,r5
0+20c <[^>]*> e68f2475 ?	sxtb16 r2,r5, ROR #8
0+210 <[^>]*> 168f2075 ?	sxtb16ne r2,r5
0+214 <[^>]*> 168f2475 ?	sxtb16ne r2,r5, ROR #8
0+218 <[^>]*> e6af2075 ?	sxtb r2,r5
0+21c <[^>]*> e6af2475 ?	sxtb r2,r5, ROR #8
0+220 <[^>]*> 16af2075 ?	sxtbne r2,r5
0+224 <[^>]*> 16af2475 ?	sxtbne r2,r5, ROR #8
0+228 <[^>]*> e6542f17 ?	uadd16	r2, r4, r7
0+22c <[^>]*> 16542f17 ?	uadd16ne	r2, r4, r7
0+230 <[^>]*> e6f32075 ?	uxtah	r2, r3, r5
0+234 <[^>]*> e6f32475 ?	uxtah	r2, r3, r5, ROR #8
0+238 <[^>]*> 16f32075 ?	uxtahne	r2, r3, r5
0+23c <[^>]*> 16f32475 ?	uxtahne	r2, r3, r5, ROR #8
0+240 <[^>]*> e6542f97 ?	uadd8	r2, r4, r7
0+244 <[^>]*> 16542f97 ?	uadd8ne	r2, r4, r7
0+248 <[^>]*> e6c32075 ?	uxtab16	r2, r3, r5
0+24c <[^>]*> e6c32475 ?	uxtab16	r2, r3, r5, ROR #8
0+250 <[^>]*> 16c32075 ?	uxtab16ne	r2, r3, r5
0+254 <[^>]*> 16c32475 ?	uxtab16ne	r2, r3, r5, ROR #8
0+258 <[^>]*> e6e32075 ?	uxtab	r2, r3, r5
0+25c <[^>]*> e6e32475 ?	uxtab	r2, r3, r5, ROR #8
0+260 <[^>]*> 16e32075 ?	uxtabne	r2, r3, r5
0+264 <[^>]*> 16e32475 ?	uxtabne	r2, r3, r5, ROR #8
0+268 <[^>]*> e6542f37 ?	uaddsubx	r2, r4, r7
0+26c <[^>]*> 16542f37 ?	uaddsubxne	r2, r4, r7
0+270 <[^>]*> e6742f17 ?	uhadd16	r2, r4, r7
0+274 <[^>]*> 16742f17 ?	uhadd16ne	r2, r4, r7
0+278 <[^>]*> e6742f97 ?	uhadd8	r2, r4, r7
0+27c <[^>]*> 16742f97 ?	uhadd8ne	r2, r4, r7
0+280 <[^>]*> e6742f37 ?	uhaddsubx	r2, r4, r7
0+284 <[^>]*> 16742f37 ?	uhaddsubxne	r2, r4, r7
0+288 <[^>]*> e6742f77 ?	uhsub16	r2, r4, r7
0+28c <[^>]*> 16742f77 ?	uhsub16ne	r2, r4, r7
0+290 <[^>]*> e6742ff7 ?	uhsub8	r2, r4, r7
0+294 <[^>]*> 16742ff7 ?	uhsub8ne	r2, r4, r7
0+298 <[^>]*> e6742f57 ?	uhsubaddx	r2, r4, r7
0+29c <[^>]*> 16742f57 ?	uhsubaddxne	r2, r4, r7
0+2a0 <[^>]*> e0421493 ?	umaal	r1, r2, r3, r4
0+2a4 <[^>]*> d0421493 ?	umaalle	r1, r2, r3, r4
0+2a8 <[^>]*> e6642f17 ?	uqadd16	r2, r4, r7
0+2ac <[^>]*> 16642f17 ?	uqadd16ne	r2, r4, r7
0+2b0 <[^>]*> e6642f97 ?	uqadd8	r2, r4, r7
0+2b4 <[^>]*> 16642f97 ?	uqadd8ne	r2, r4, r7
0+2b8 <[^>]*> e6642f37 ?	uqaddsubx	r2, r4, r7
0+2bc <[^>]*> 16642f37 ?	uqaddsubxne	r2, r4, r7
0+2c0 <[^>]*> e6642f77 ?	uqsub16	r2, r4, r7
0+2c4 <[^>]*> 16642f77 ?	uqsub16ne	r2, r4, r7
0+2c8 <[^>]*> e6642ff7 ?	uqsub8	r2, r4, r7
0+2cc <[^>]*> 16642ff7 ?	uqsub8ne	r2, r4, r7
0+2d0 <[^>]*> e6642f57 ?	uqsubaddx	r2, r4, r7
0+2d4 <[^>]*> 16642f57 ?	uqsubaddxne	r2, r4, r7
0+2d8 <[^>]*> e781f312 ?	usad8	r1, r2, r3
0+2dc <[^>]*> 1781f312 ?	usad8ne	r1, r2, r3
0+2e0 <[^>]*> e7814312 ?	usada8	r1, r2, r3, r4
0+2e4 <[^>]*> 17814312 ?	usada8ne	r1, r2, r3, r4
0+2e8 <[^>]*> e6ef1012 ?	usat	r1, #15, r2
0+2ec <[^>]*> e6ef1252 ?	usat	r1, #15, r2, ASR #4
0+2f0 <[^>]*> e6ef1212 ?	usat	r1, #15, r2, LSL #4
0+2f4 <[^>]*> e6ef1f32 ?	usat16	r1, #15, r2
0+2f8 <[^>]*> d6ef1f32 ?	usat16le	r1, #15, r2
0+2fc <[^>]*> d6ef1012 ?	usatle	r1, #15, r2
0+300 <[^>]*> d6ef1252 ?	usatle	r1, #15, r2, ASR #4
0+304 <[^>]*> d6ef1212 ?	usatle	r1, #15, r2, LSL #4
0+308 <[^>]*> e6542f77 ?	usub16	r2, r4, r7
0+30c <[^>]*> 16542f77 ?	usub16ne	r2, r4, r7
0+310 <[^>]*> e6542ff7 ?	usub8	r2, r4, r7
0+314 <[^>]*> 16542ff7 ?	usub8ne	r2, r4, r7
0+318 <[^>]*> e6542f57 ?	usubaddx	r2, r4, r7
0+31c <[^>]*> 16542f57 ?	usubaddxne	r2, r4, r7
0+320 <[^>]*> e6ff2075 ?	uxth r2,r5
0+324 <[^>]*> e6ff2475 ?	uxth r2,r5, ROR #8
0+328 <[^>]*> 16ff2075 ?	uxthne r2,r5
0+32c <[^>]*> 16ff2475 ?	uxthne r2,r5, ROR #8
0+330 <[^>]*> e6cf2075 ?	uxtb16 r2,r5
0+334 <[^>]*> e6cf2475 ?	uxtb16 r2,r5, ROR #8
0+338 <[^>]*> 16cf2075 ?	uxtb16ne r2,r5
0+33c <[^>]*> 16cf2475 ?	uxtb16ne r2,r5, ROR #8
0+340 <[^>]*> e6ef2075 ?	uxtb r2,r5
0+344 <[^>]*> e6ef2475 ?	uxtb r2,r5, ROR #8
0+348 <[^>]*> 16ef2075 ?	uxtbne r2,r5
0+34c <[^>]*> 16ef2475 ?	uxtbne r2,r5, ROR #8
