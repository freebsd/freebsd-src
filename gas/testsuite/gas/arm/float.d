# name: Core floating point instructions
# as: -mcpu=arm7tdmi -mfpu=fpa
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]+> ee088101 ?	mvfe	f0, f1
0+004 <[^>]+> 0e08b105 ?	mvfeqe	f3, f5
0+008 <[^>]+> 0e00c189 ?	mvfeqd	f4, #1\.0
0+00c <[^>]+> ee00c107 ?	mvfs	f4, f7
0+010 <[^>]+> ee008121 ?	mvfsp	f0, f1
0+014 <[^>]+> ee00b1c4 ?	mvfdm	f3, f4
0+018 <[^>]+> ee08f167 ?	mvfez	f7, f7
0+01c <[^>]+> ee09010a ?	adfe	f0, f1, #2\.0
0+020 <[^>]+> 0e0a110e ?	adfeqe	f1, f2, #0\.5
0+024 <[^>]+> ee043145 ?	adfsm	f3, f4, f5
0+028 <[^>]+> ee20018a ?	sufd	f0, f0, #2\.0
0+02c <[^>]+> ee22110f ?	sufs	f1, f2, #10\.0
0+030 <[^>]+> 1e2c3165 ?	sufneez	f3, f4, f5
0+034 <[^>]+> ee311108 ?	rsfs	f1, f1, #0\.0
0+038 <[^>]+> ee3031ad ?	rsfdp	f3, f0, #5\.0
0+03c <[^>]+> de367180 ?	rsfled	f7, f6, f0
0+040 <[^>]+> ee100180 ?	mufd	f0, f0, f0
0+044 <[^>]+> ee1a116b ?	mufez	f1, f2, #3\.0
0+048 <[^>]+> ee10010c ?	mufs	f0, f0, #4\.0
0+04c <[^>]+> ee400189 ?	dvfd	f0, f0, #1\.0
0+050 <[^>]+> ee49016f ?	dvfez	f0, f1, #10\.0
0+054 <[^>]+> 4e443145 ?	dvfmism	f3, f4, f5
0+058 <[^>]+> ee59010f ?	rdfe	f0, f1, #10\.0
0+05c <[^>]+> ee573109 ?	rdfs	f3, f7, #1\.0
0+060 <[^>]+> 3e5441a3 ?	rdfccdp	f4, f4, f3
0+064 <[^>]+> ee620183 ?	powd	f0, f2, f3
0+068 <[^>]+> ee63110f ?	pows	f1, f3, #10\.0
0+06c <[^>]+> 2e6f4169 ?	powcsez	f4, f7, #1\.0
0+070 <[^>]+> ee767107 ?	rpws	f7, f6, f7
0+074 <[^>]+> 0e710182 ?	rpweqd	f0, f1, f2
0+078 <[^>]+> ee7a2143 ?	rpwem	f2, f2, f3
0+07c <[^>]+> ee82118b ?	rmfd	f1, f2, #3\.0
0+080 <[^>]+> 6e843104 ?	rmfvss	f3, f4, f4
0+084 <[^>]+> ee8f4120 ?	rmfep	f4, f7, f0
0+088 <[^>]+> ee910102 ?	fmls	f0, f1, f2
0+08c <[^>]+> 0e931105 ?	fmleqs	f1, f3, f5
0+090 <[^>]+> 5e964160 ?	fmlplsz	f4, f6, f0
0+094 <[^>]+> eea3110f ?	fdvs	f1, f3, #10\.0
0+098 <[^>]+> eea10122 ?	fdvsp	f0, f1, f2
0+09c <[^>]+> 2ea44144 ?	fdvcssm	f4, f4, f4
0+0a0 <[^>]+> eeb11109 ?	frds	f1, f1, #1\.0
0+0a4 <[^>]+> ceb12100 ?	frdgts	f2, f1, f0
0+0a8 <[^>]+> ceb44165 ?	frdgtsz	f4, f4, f5
0+0ac <[^>]+> eec10182 ?	pold	f0, f1, f2
0+0b0 <[^>]+> eec6416b ?	polsz	f4, f6, #3\.0
0+0b4 <[^>]+> 0ece5107 ?	poleqe	f5, f6, f7
0+0b8 <[^>]+> ee108101 ?	mnfs	f0, f1
0+0bc <[^>]+> ee10818b ?	mnfd	f0, #3\.0
0+0c0 <[^>]+> ee18816c ?	mnfez	f0, #4\.0
0+0c4 <[^>]+> 0e188165 ?	mnfeqez	f0, f5
0+0c8 <[^>]+> ee108124 ?	mnfsp	f0, f4
0+0cc <[^>]+> ee1091c7 ?	mnfdm	f1, f7
0+0d0 <[^>]+> ee208181 ?	absd	f0, f1
0+0d4 <[^>]+> ee20912b ?	abssp	f1, #3\.0
0+0d8 <[^>]+> 0e28c105 ?	abseqe	f4, f5
0+0dc <[^>]+> ee309102 ?	rnds	f1, f2
0+0e0 <[^>]+> ee30b184 ?	rndd	f3, f4
0+0e4 <[^>]+> 0e38e16c ?	rndeqez	f6, #4\.0
0+0e8 <[^>]+> ee40d105 ?	sqts	f5, f5
0+0ec <[^>]+> ee40e1a6 ?	sqtdp	f6, f6
0+0f0 <[^>]+> 5e48f166 ?	sqtplez	f7, f6
0+0f4 <[^>]+> ee50810f ?	logs	f0, #10\.0
0+0f8 <[^>]+> ee58810f ?	loge	f0, #10\.0
0+0fc <[^>]+> 1e5081e1 ?	lognedz	f0, f1
0+100 <[^>]+> ee689102 ?	lgne	f1, f2
0+104 <[^>]+> ee6091e3 ?	lgndz	f1, f3
0+108 <[^>]+> 7e60b104 ?	lgnvcs	f3, f4
0+10c <[^>]+> ee709103 ?	exps	f1, f3
0+110 <[^>]+> ee78b14f ?	expem	f3, #10\.0
0+114 <[^>]+> 5e70e187 ?	exppld	f6, f7
0+118 <[^>]+> ee808181 ?	sind	f0, f1
0+11c <[^>]+> ee809142 ?	sinsm	f1, f2
0+120 <[^>]+> ce88c10d ?	singte	f4, #5\.0
0+124 <[^>]+> ee909183 ?	cosd	f1, f3
0+128 <[^>]+> ee98c145 ?	cosem	f4, f5
0+12c <[^>]+> 1e90e1a1 ?	cosnedp	f6, f1
0+130 <[^>]+> eea89105 ?	tane	f1, f5
0+134 <[^>]+> eea0c167 ?	tansz	f4, f7
0+138 <[^>]+> aea091ec ?	tangedz	f1, #4\.0
0+13c <[^>]+> eeb8c105 ?	asne	f4, f5
0+140 <[^>]+> eeb0e12e ?	asnsp	f6, #0\.5
0+144 <[^>]+> 4eb0d1e5 ?	asnmidz	f5, f5
0+148 <[^>]+> eec0d106 ?	acss	f5, f6
0+14c <[^>]+> eec0e180 ?	acsd	f6, f0
0+150 <[^>]+> 2ec8914e ?	acscsem	f1, #0\.5
0+154 <[^>]+> eed88105 ?	atne	f0, f5
0+158 <[^>]+> eed0916d ?	atnsz	f1, #5\.0
0+15c <[^>]+> bed0b182 ?	atnltd	f3, f2
0+160 <[^>]+> eee8d104 ?	urde	f5, f4
0+164 <[^>]+> eef8e105 ?	nrme	f6, f5
0+168 <[^>]+> 5ef0f1e5 ?	nrmpldz	f7, f5
0+16c <[^>]+> ee008130 ?	fltsp	f0, r8
0+170 <[^>]+> ee090110 ?	flte	f1, r0
0+174 <[^>]+> 0e0571f0 ?	flteqdz	f5, r7
0+178 <[^>]+> ee100111 ?	fix	r0, f1
0+17c <[^>]+> ee101177 ?	fixz	r1, f7
0+180 <[^>]+> 2e105155 ?	fixcsm	r5, f5
0+184 <[^>]+> ee400110 ?	wfc	r0
0+188 <[^>]+> ee201110 ?	wfs	r1
0+18c <[^>]+> 0e302110 ?	rfseq	r2
0+190 <[^>]+> ee504110 ?	rfc	r4
0+194 <[^>]+> ee90f119 ?	cmf	f0, #1\.0
0+198 <[^>]+> ee91f112 ?	cmf	f1, f2
0+19c <[^>]+> 0e90f111 ?	cmfeq	f0, f1
0+1a0 <[^>]+> eeb0f11b ?	cnf	f0, #3\.0
0+1a4 <[^>]+> eeb1f11e ?	cnf	f1, #0\.5
0+1a8 <[^>]+> 6eb3f114 ?	cnfvs	f3, f4
0+1ac <[^>]+> eed0f111 ?	cmfe	f0, f1
0+1b0 <[^>]+> 0ed1f112 ?	cmfeeq	f1, f2
0+1b4 <[^>]+> 0ed3f11d ?	cmfeeq	f3, #5\.0
0+1b8 <[^>]+> eef1f113 ?	cnfe	f1, f3
0+1bc <[^>]+> 0ef3f114 ?	cnfeeq	f3, f4
0+1c0 <[^>]+> 0ef4f117 ?	cnfeeq	f4, f7
0+1c4 <[^>]+> eef4f11d ?	cnfe	f4, #5\.0
0+1c8 <[^>]+> ed900200 ?	lfm	f0, 4, \[r0\]
0+1cc <[^>]+> ed900200 ?	lfm	f0, 4, \[r0\]
0+1d0 <[^>]+> ed911210 ?	lfm	f1, 4, \[r1, #64\]
0+1d4 <[^>]+> edae22ff ?	sfm	f2, 4, \[lr, #1020\]!
0+1d8 <[^>]+> 0c68f2ff ?	sfmeq	f7, 3, \[r8\], #-1020
0+1dc <[^>]+> eddf6200 ?	lfm	f6, 2, \[pc\]
0+1e0 <[^>]+> eca8f203 ?	sfm	f7, 1, \[r8\], #12
0+1e4 <[^>]+> 0d16520c ?	lfmeq	f5, 4, \[r6, #-48\]
0+1e8 <[^>]+> 1d42c209 ?	sfmne	f4, 3, \[r2, #-36\]
0+1ec <[^>]+> 1d62c209 ?	sfmne	f4, 3, \[r2, #-36\]!
