#objdump: -dr
#name: D30V basic instruction test
#as:

.*: +file format elf32-d30v

Disassembly of section .text:

00000000 <start>:
   0:	08815a80 88001083 	abs	r21, r42	->	add.s	r1, r2, r3
   8:	080b2cda 00f00000 	add.s	r50, r51, 0x1a	||	nop	
  10:	880b2cf7 8ab1beef 	add.l	r50, r51, 0xdeadbeef
  18:	08101083 881b2cda 	add2h.s	r1, r2, r3	->	add2h.s	r50, r51, 0x1a
  20:	881b2cf7 8ab1beef 	add2h.l	r50, r51, 0xdeadbeef
  28:	08401083 884b2cda 	addc.s	r1, r2, r3	->	addc.s	r50, r51, 0x1a
  30:	884b2cf7 8ab1beef 	addc.l	r50, r51, 0xdeadbeef
  38:	09001083 890b2cda 	addhlll.s	r1, r2, r3	->	addhlll.s	r50, r51, 0x1a
  40:	890b2cf7 8ab1beef 	addhlll.l	r50, r51, 0xdeadbeef
  48:	09101083 891b2cda 	addhllh.s	r1, r2, r3	->	addhllh.s	r50, r51, 0x1a
  50:	891b2cf7 8ab1beef 	addhllh.l	r50, r51, 0xdeadbeef
  58:	09201083 892b2cda 	addhlhl.s	r1, r2, r3	->	addhlhl.s	r50, r51, 0x1a
  60:	892b2cf7 8ab1beef 	addhlhl.l	r50, r51, 0xdeadbeef
  68:	09301083 893b2cda 	addhlhh.s	r1, r2, r3	->	addhlhh.s	r50, r51, 0x1a
  70:	893b2cf7 8ab1beef 	addhlhh.l	r50, r51, 0xdeadbeef
  78:	09401083 894b2cda 	addhhll.s	r1, r2, r3	->	addhhll.s	r50, r51, 0x1a
  80:	894b2cf7 8ab1beef 	addhhll.l	r50, r51, 0xdeadbeef
  88:	09501083 895b2cda 	addhhlh.s	r1, r2, r3	->	addhhlh.s	r50, r51, 0x1a
  90:	895b2cf7 8ab1beef 	addhhlh.l	r50, r51, 0xdeadbeef
  98:	09601083 896b2cda 	addhhhl.s	r1, r2, r3	->	addhhhl.s	r50, r51, 0x1a
  a0:	896b2cf7 8ab1beef 	addhhhl.l	r50, r51, 0xdeadbeef
  a8:	09701083 897b2cda 	addhhhh.s	r1, r2, r3	->	addhhhh.s	r50, r51, 0x1a
  b0:	897b2cf7 8ab1beef 	addhhhh.l	r50, r51, 0xdeadbeef
  b8:	08601083 886b2cda 	adds.s	r1, r2, r3	->	adds.s	r50, r51, 0x1a
  c0:	886b2cf7 8ab1beef 	adds.l	r50, r51, 0xdeadbeef
  c8:	08701083 887b2cda 	adds2h.s	r1, r2, r3	->	adds2h.s	r50, r51, 0x1a
  d0:	887b2cf7 8ab1beef 	adds2h.l	r50, r51, 0xdeadbeef
  d8:	03801083 838b2cda 	and.s	r1, r2, r3	->	and.s	r50, r51, 0x1a
  e0:	838b2cf7 8ab1beef 	and.l	r50, r51, 0xdeadbeef
  e8:	02800042 82883105 	andfg	f0, f1, f2	->	andfg	f3, s, 0x5
  f0:	08a01083 88a84146 	avg.s	r1, r2, r3	->	avg.s	r4, r5, 0x6
  f8:	88ab2cf7 8ab1beef 	avg.l	r50, r51, 0xdeadbeef
 100:	08b01083 88b84146 	avg2h.s	r1, r2, r3	->	avg2h.s	r4, r5, 0x6
 108:	88bb2cf7 8ab1beef 	avg2h.l	r50, r51, 0xdeadbeef
 110:	02301083 82384146 	bclr	r1, r2, r3	->	bclr	r4, r5, 0x6
 118:	02101083 82185cc6 	bnot	r1, r2, r3	->	bnot	r5, r51, 0x6
 120:	00000029 00f00000 	bra.s	r41	||	nop	
 128:	00080008 00f00000 	bra.s	40	\(168 <start\+0x168>\)	||	nop	
 130:	00081e01 00f00000 	bra.s	f008	\(f138 <start\+0xf138>\)	||	nop	
 138:	0046902a 00f00000 	bratnz.s	r41, r42	||	nop	
 140:	804c1000 8000f00d 	bratnz.l	r1, f00d	\(f14d <start\+0xf14d>\)
 148:	804c1037 8ab1f00d 	bratnz.l	r1, -21520ff3	\(deadf155 <start\+0xdeadf155>\)
 150:	0042902a 00f00000 	bratzr.s	r41, r42	||	nop	
 158:	80481000 8000f00d 	bratzr.l	r1, f00d	\(f165 <start\+0xf165>\)
 160:	80481037 8ab1f00d 	bratzr.l	r1, -21520ff3	\(deadf16d <start\+0xdeadf16d>\)
 168:	02201083 82285cc6 	bset	r1, r2, r3	->	bset	r5, r51, 0x6
 170:	00200029 00f00000 	bsr.s	r41	||	nop	
 178:	00281e01 00f00000 	bsr.s	f008	\(f180 <start\+0xf180>\)	||	nop	
 180:	80280037 8ab1f00d 	bsr.l	-21520ff3	\(deadf18d <start\+0xdeadf18d>\)
 188:	0066902a 00f00000 	bsrtnz.s	r41, r42	||	nop	
 190:	806c1000 8000f00d 	bsrtnz.l	r1, f00d	\(f19d <start\+0xf19d>\)
 198:	806c1037 8ab1f00d 	bsrtnz.l	r1, -21520ff3	\(deadf1a5 <start\+0xdeadf1a5>\)
 1a0:	0062902a 00f00000 	bsrtzr.s	r41, r42	||	nop	
 1a8:	80681000 8000f00d 	bsrtzr.l	r1, f00d	\(f1b5 <start\+0xf1b5>\)
 1b0:	80681037 8ab1f00d 	bsrtzr.l	r1, -21520ff3	\(deadf1bd <start\+0xdeadf1bd>\)
 1b8:	02001083 82085cc6 	btst	f1, r2, r3	->	btst	v, r51, 0x6
 1c0:	02c000c1 82c09515 	cmpeq.s	f0, r3, r1	->	cmpne.s	f1, r20, r21
 1c8:	02c127e0 82c1b0c4 	cmpgt.s	f2, r31, r32	->	cmpge.s	f3, r3, r4
 1d0:	02c240c4 82c2d0c4 	cmplt.s	s, r3, r4	->	cmple.s	v, r3, r4
 1d8:	02c360c4 82c3f0c4 	cmpps.s	va, r3, r4	->	cmpng.s	c, r3, r4
 1e0:	02d127e0 82d1b0c4 	cmpugt.s	f2, r31, r32	->	cmpuge.s	f3, r3, r4
 1e8:	02d240c4 82d2d0c4 	cmpult.s	s, r3, r4	->	cmpule.s	v, r3, r4
 1f0:	01001008 81081020 	dbra.s	r1, r8	->	dbra.s	r1, 100	\(2f0 <start\+0x2f0>\)
 1f8:	81081037 8ab1f00d 	dbra.l	r1, -21520ff3	\(deadf205 <start\+0xdeadf205>\)
 200:	0140201f 81482020 	dbrai.s	10	\(.*\), r31	->	dbrai.s	10	\(.*\), 100	\(300 <start\+0x300>\)
 208:	81482037 8ab1f00d 	dbrai.l	10	\(.*\), -21520ff3	\(deadf215 <start\+0xdeadf215>\)
 210:	01201008 00f00000 	dbsr.s	r1, r8	||	nop	
 218:	01281020 00f00000 	dbsr.s	r1, 100	\(318 <start\+0x318>\)	||	nop	
 220:	81281037 8ab1f00d 	dbsr.l	r1, -21520ff3	\(deadf22d <start\+0xdeadf22d>\)
 228:	0160401f 00f00000 	dbsri.s	20, r31	||	nop	
 230:	01684020 00f00000 	dbsri.s	20, 100	\(330 <start\+0x330>\)	||	nop	
 238:	81684037 8ab1f00d 	dbsri.l	20	\(.*\), -21520ff3	\(deadf245 <start\+0xdeadf245>\)
 240:	01101020 00f00000 	djmp.s	r1, r32	||	nop	
 248:	81181000 8000f00d 	djmp.l	r1, f00d <start\+0xf00d>
 250:	81181037 8ab1f00d 	djmp.l	r1, deadf00d <start\+0xdeadf00d>
 258:	01506020 00f00000 	djmpi.s	30, r32	||	nop	
 260:	81586000 8000f00d 	djmpi.l	30	\(.*\), f00d <start\+0xf00d>
 268:	81586037 8ab1f00d 	djmpi.l	30	\(.*\), deadf00d <start\+0xdeadf00d>
 270:	01301020 00f00000 	djsr.s	r1, r32	||	nop	
 278:	81381000 8000f00d 	djsr.l	r1, f00d <start\+0xf00d>
 280:	81381037 8ab1f00d 	djsr.l	r1, deadf00d <start\+0xdeadf00d>
 288:	01702020 00f00000 	djsri.s	10, r32	||	nop	
 290:	81784000 8000f00d 	djsri.l	20	\(.*\), f00d <start\+0xf00d>
 298:	81788037 8ab1f00d 	djsri.l	40	\(.*\), deadf00d <start\+0xdeadf00d>
 2a0:	00100029 00f00000 	jmp.s	r41	||	nop	
 2a8:	00181e01 00f00000 	jmp.s	f008 <start\+0xf008>	||	nop	
 2b0:	80180037 8ab1f00d 	jmp.l	deadf00d <start\+0xdeadf00d>
 2b8:	0056902a 00f00000 	jmptnz.s	r41, r42	||	nop	
 2c0:	805c1000 8000f00d 	jmptnz.l	r1, f00d <start\+0xf00d>
 2c8:	805c1037 8ab1f00d 	jmptnz.l	r1, deadf00d <start\+0xdeadf00d>
 2d0:	0052902a 00f00000 	jmptzr.s	r41, r42	||	nop	
 2d8:	80581000 8000f00d 	jmptzr.l	r1, f00d <start\+0xf00d>
 2e0:	80581037 8ab1f00d 	jmptzr.l	r1, deadf00d <start\+0xdeadf00d>
 2e8:	08c01084 88c8108f 	joinll.s	r1, r2, r4	->	joinll.s	r1, r2, 0xf
 2f0:	88c810b7 8ab1f00d 	joinll.l	r1, r2, 0xdeadf00d
 2f8:	08d01084 88d8108f 	joinlh.s	r1, r2, r4	->	joinlh.s	r1, r2, 0xf
 300:	88d810b7 8ab1f00d 	joinlh.l	r1, r2, 0xdeadf00d
 308:	08e01084 88e8108f 	joinhl.s	r1, r2, r4	->	joinhl.s	r1, r2, 0xf
 310:	88e810b7 8ab1f00d 	joinhl.l	r1, r2, 0xdeadf00d
 318:	08f01084 88f8108f 	joinhh.s	r1, r2, r4	->	joinhh.s	r1, r2, 0xf
 320:	88f810b7 8ab1f00d 	joinhh.l	r1, r2, 0xdeadf00d
 328:	00300029 00f00000 	jsr.s	r41	||	nop	
 330:	00381e01 00f00000 	jsr.s	f008 <start\+0xf008>	||	nop	
 338:	80380037 8ab1f00d 	jsr.l	deadf00d <start\+0xdeadf00d>
 340:	0076902a 00f00000 	jsrtnz.s	r41, r42	||	nop	
 348:	807c1000 8000f00d 	jsrtnz.l	r1, f00d <start\+0xf00d>
 350:	807c1037 8ab1f00d 	jsrtnz.l	r1, deadf00d <start\+0xdeadf00d>
 358:	0072902a 00f00000 	jsrtzr.s	r41, r42	||	nop	
 360:	80781000 8000f00d 	jsrtzr.l	r1, f00d <start\+0xf00d>
 368:	80781037 8ab1f00d 	jsrtzr.l	r1, deadf00d <start\+0xdeadf00d>
 370:	043061c8 843461c8 	ld2h.s	r6, @\(r7, r8\)	->	ld2h.s	r6, @\(r7\+, r8\)
 378:	043c61c8 843861da 	ld2h.s	r6, @\(r7-, r8\)	->	ld2h.s	r6, @\(r7, 0x1a\)
 380:	843861c0 80001234 	ld2h.l	r6, @\(r7, 0x1234\)
 388:	046061c8 846461c8 	ld2w.s	r6, @\(r7, r8\)	->	ld2w.s	r6, @\(r7\+, r8\)
 390:	046c61c8 846861da 	ld2w.s	r6, @\(r7-, r8\)	->	ld2w.s	r6, @\(r7, 0x1a\)
 398:	846861c0 80001234 	ld2w.l	r6, @\(r7, 0x1234\)
 3a0:	045061c8 845461c8 	ld4bh.s	r6, @\(r7, r8\)	->	ld4bh.s	r6, @\(r7\+, r8\)
 3a8:	045c61c8 845861da 	ld4bh.s	r6, @\(r7-, r8\)	->	ld4bh.s	r6, @\(r7, 0x1a\)
 3b0:	845861c0 80001234 	ld4bh.l	r6, @\(r7, 0x1234\)
 3b8:	04d061c8 84d461c8 	ld4bhu.s	r6, @\(r7, r8\)	->	ld4bhu.s	r6, @\(r7\+, r8\)
 3c0:	04dc61c8 84d861da 	ld4bhu.s	r6, @\(r7-, r8\)	->	ld4bhu.s	r6, @\(r7, 0x1a\)
 3c8:	84d861c0 80001234 	ld4bhu.l	r6, @\(r7, 0x1234\)
 3d0:	040061c8 840461c8 	ldb.s	r6, @\(r7, r8\)	->	ldb.s	r6, @\(r7\+, r8\)
 3d8:	040c61c8 840861da 	ldb.s	r6, @\(r7-, r8\)	->	ldb.s	r6, @\(r7, 0x1a\)
 3e0:	840861c0 80001234 	ldb.l	r6, @\(r7, 0x1234\)
 3e8:	049061c8 849461c8 	ldbu.s	r6, @\(r7, r8\)	->	ldbu.s	r6, @\(r7\+, r8\)
 3f0:	049c61c8 849861da 	ldbu.s	r6, @\(r7-, r8\)	->	ldbu.s	r6, @\(r7, 0x1a\)
 3f8:	849861c0 80001234 	ldbu.l	r6, @\(r7, 0x1234\)
 400:	042061c8 842461c8 	ldh.s	r6, @\(r7, r8\)	->	ldh.s	r6, @\(r7\+, r8\)
 408:	042c61c8 842861da 	ldh.s	r6, @\(r7-, r8\)	->	ldh.s	r6, @\(r7, 0x1a\)
 410:	842861c0 80001234 	ldh.l	r6, @\(r7, 0x1234\)
 418:	041061c8 841461c8 	ldhh.s	r6, @\(r7, r8\)	->	ldhh.s	r6, @\(r7\+, r8\)
 420:	041c61c8 841861da 	ldhh.s	r6, @\(r7-, r8\)	->	ldhh.s	r6, @\(r7, 0x1a\)
 428:	841861c0 80001234 	ldhh.l	r6, @\(r7, 0x1234\)
 430:	04a061c8 84a461c8 	ldhu.s	r6, @\(r7, r8\)	->	ldhu.s	r6, @\(r7\+, r8\)
 438:	04ac61c8 84a861da 	ldhu.s	r6, @\(r7-, r8\)	->	ldhu.s	r6, @\(r7, 0x1a\)
 440:	84a861c0 80001234 	ldhu.l	r6, @\(r7, 0x1234\)
 448:	044061c8 844461c8 	ldw.s	r6, @\(r7, r8\)	->	ldw.s	r6, @\(r7\+, r8\)
 450:	044c61c8 844861da 	ldw.s	r6, @\(r7-, r8\)	->	ldw.s	r6, @\(r7, 0x1a\)
 458:	844861c0 80001234 	ldw.l	r6, @\(r7, 0x1234\)
 460:	8b48109f 0b401084 	mac0	r1, r2, 0x1f	<-	mac0	r1, r2, r4
 468:	8b4c109f 0b441084 	mac1	r1, r2, 0x1f	<-	mac1	r1, r2, r4
 470:	8b58109f 0b501084 	macs0	r1, r2, 0x1f	<-	macs0	r1, r2, r4
 478:	8b5c109f 0b541084 	macs1	r1, r2, 0x1f	<-	macs1	r1, r2, r4
 480:	047c004a 8474004a 	moddec	r1, 0xa	->	modinc	r1, 0xa
 488:	8b68109f 0b601084 	msub0	r1, r2, 0x1f	<-	msub0	r1, r2, r4
 490:	8b6c109f 0b641084 	msub1	r1, r2, 0x1f	<-	msub1	r1, r2, r4
 498:	8b08108a 0b001084 	mul	r1, r2, 0xa	<-	mul	r1, r2, r4
 4a0:	8b78109f 0b701084 	msubs0	r1, r2, 0x1f	<-	msubs0	r1, r2, r4
 4a8:	8b7c109f 0b741084 	msubs1	r1, r2, 0x1f	<-	msubs1	r1, r2, r4
 4b0:	00f00000 00f00000 	nop		||	nop	
 4b8:	8a08108a 0a001084 	mul2h	r1, r2, 0xa	<-	mul2h	r1, r2, r4
 4c0:	8a48108a 0a401084 	mulhxll	r1, r2, 0xa	<-	mulhxll	r1, r2, r4
 4c8:	8a58108a 0a501084 	mulhxlh	r1, r2, 0xa	<-	mulhxlh	r1, r2, r4
 4d0:	8a68108a 0a601084 	mulhxhl	r1, r2, 0xa	<-	mulhxhl	r1, r2, r4
 4d8:	8a78108a 0a701084 	mulhxhh	r1, r2, 0xa	<-	mulhxhh	r1, r2, r4
 4e0:	8b900044 0a108084 	mulxs	a0, r1, r4	<-	mulx2h	r8, r2, r4
 4e8:	8b88108a 0b800044 	mulx	a1, r2, 0xa	<-	mulx	a0, r1, r4
 4f0:	8bf8204a 0bf01004 	mvfacc	r2, a1, 0xa	<-	mvfacc	r1, a0, r4
 4f8:	8b98108a 0a18808a 	mulxs	a1, r2, 0xa	<-	mulx2h	r8, r2, 0xa
 500:	01e0a080 00f00000 	mvfsys	r10, pc	||	nop	
 508:	01e0a1c0 00f00000 	mvfsys	r10, rpt_c	||	nop	
 510:	01e0a000 00f00000 	mvfsys	r10, psw	||	nop	
 518:	01e0a002 00f00000 	mvfsys	r10, pswh	||	nop	
 520:	01e0a001 00f00000 	mvfsys	r10, pswl	||	nop	
 528:	01e0a003 00f00000 	mvfsys	r10, f0	||	nop	
 530:	01e0a103 00f00000 	mvfsys	r10, s	||	nop	
 538:	80e07280 0af01084 	mvtsys	rpt_c, r10	<-	mvtacc	a1, r2, r4
 540:	00e00280 00f00000 	mvtsys	psw, r10	||	nop	
 548:	00e00282 00f00000 	mvtsys	pswh, r10	||	nop	
 550:	00e00281 00f00000 	mvtsys	pswl, r10	||	nop	
 558:	00e00283 00f00000 	mvtsys	f0, r10	||	nop	
 560:	00e03283 00f00000 	mvtsys	f3, r10	||	nop	
 568:	00e04283 00f00000 	mvtsys	s, r10	||	nop	
 570:	00e05283 00f00000 	mvtsys	v, r10	||	nop	
 578:	00e06283 00f00000 	mvtsys	va, r10	||	nop	
 580:	00e07283 00f00000 	mvtsys	c, r10	||	nop	
 588:	00f00000 83901080 	nop		->	not	r1, r2
 590:	02901080 83a01084 	notfg	f1, f2	->	or.s	r1, r2, r4
 598:	03a8109a 00f00000 	or.s	r1, r2, 0x1a	||	nop	
 5a0:	83a810b7 8ab1f00d 	or.l	r1, r2, 0xdeadf00d
 5a8:	02a01084 82a84081 	orfg	f1, f2, s	->	orfg	s, f2, 0x1
 5b0:	00800000 00f00000 	reit		||	nop	
 5b8:	01801002 00f00000 	repeat.s	r1, r2	||	nop	
 5c0:	81884000 8000dead 	repeat.l	r4, dead	\(e46d <start\+0xe46d>\)
 5c8:	81884037 8ab1f00d 	repeat.l	r4, -21520ff3	\(deadf5d5 <start\+0xdeadf5d5>\)
 5d0:	01a0a001 00f00000 	repeati.s	0xa, r1	||	nop	
 5d8:	01a8a200 00f00000 	repeati.s	0xa, 1000	\(15d8 <start\+0x15d8>\)	||	nop	
 5e0:	00f00000 00f00000 	nop		||	nop	
 5e8:	03401084 8348108a 	rot	r1, r2, r4	->	rot	r1, r2, 0xa
 5f0:	03501084 8358108a 	rot2h	r1, r2, r4	->	rot2h	r1, r2, 0xa
 5f8:	8a88108a 0a801084 	sat	r1, r2, 0xa	<-	sat	r1, r2, r4
 600:	8a98108a 0a901084 	sat2h	r1, r2, 0xa	<-	sat2h	r1, r2, r4
 608:	8bc8108a 0bc01084 	sathl	r1, r2, 0xa	<-	sathl	r1, r2, r4
 610:	8bd8108a 0bd01084 	sathh	r1, r2, 0xa	<-	sathh	r1, r2, r4
 618:	8aa8108a 0aa01084 	satz	r1, r2, 0xa	<-	satz	r1, r2, r4
 620:	8ab8108a 0ab01084 	satz2h	r1, r2, 0xa	<-	satz2h	r1, r2, r4
 628:	03001084 8308108a 	sra	r1, r2, r4	->	sra	r1, r2, 0xa
 630:	03101084 8318108a 	sra2h	r1, r2, r4	->	sra2h	r1, r2, 0xa
 638:	03601084 8368108a 	src	r1, r2, r4	->	src	r1, r2, 0xa
 640:	03201084 8328108a 	srl	r1, r2, r4	->	srl	r1, r2, 0xa
 648:	03301084 8338108a 	srl2h	r1, r2, r4	->	srl2h	r1, r2, 0xa
 650:	053061c8 853461c8 	st2h.s	r6, @\(r7, r8\)	->	st2h.s	r6, @\(r7\+, r8\)
 658:	053c61c8 853861da 	st2h.s	r6, @\(r7-, r8\)	->	st2h.s	r6, @\(r7, 0x1a\)
 660:	853861c0 80001234 	st2h.l	r6, @\(r7, 0x1234\)
 668:	056061c8 856461c8 	st2w.s	r6, @\(r7, r8\)	->	st2w.s	r6, @\(r7\+, r8\)
 670:	056c61c8 856861da 	st2w.s	r6, @\(r7-, r8\)	->	st2w.s	r6, @\(r7, 0x1a\)
 678:	856861c0 80001234 	st2w.l	r6, @\(r7, 0x1234\)
 680:	055061c8 855461c8 	st4hb.s	r6, @\(r7, r8\)	->	st4hb.s	r6, @\(r7\+, r8\)
 688:	055c61c8 855861da 	st4hb.s	r6, @\(r7-, r8\)	->	st4hb.s	r6, @\(r7, 0x1a\)
 690:	855861c0 80001234 	st4hb.l	r6, @\(r7, 0x1234\)
 698:	050061c8 850461c8 	stb.s	r6, @\(r7, r8\)	->	stb.s	r6, @\(r7\+, r8\)
 6a0:	050c61c8 850861da 	stb.s	r6, @\(r7-, r8\)	->	stb.s	r6, @\(r7, 0x1a\)
 6a8:	850861c0 80001234 	stb.l	r6, @\(r7, 0x1234\)
 6b0:	052061c8 852461c8 	sth.s	r6, @\(r7, r8\)	->	sth.s	r6, @\(r7\+, r8\)
 6b8:	052c61c8 852861da 	sth.s	r6, @\(r7-, r8\)	->	sth.s	r6, @\(r7, 0x1a\)
 6c0:	852861c0 80001234 	sth.l	r6, @\(r7, 0x1234\)
 6c8:	051061c8 851461c8 	sthh.s	r6, @\(r7, r8\)	->	sthh.s	r6, @\(r7\+, r8\)
 6d0:	051c61c8 851861da 	sthh.s	r6, @\(r7-, r8\)	->	sthh.s	r6, @\(r7, 0x1a\)
 6d8:	851861c0 80001234 	sthh.l	r6, @\(r7, 0x1234\)
 6e0:	054061c8 854461c8 	stw.s	r6, @\(r7, r8\)	->	stw.s	r6, @\(r7\+, r8\)
 6e8:	054c61c8 854861da 	stw.s	r6, @\(r7-, r8\)	->	stw.s	r6, @\(r7, 0x1a\)
 6f0:	854861c0 80001234 	stw.l	r6, @\(r7, 0x1234\)
 6f8:	08201083 882b2cda 	sub.s	r1, r2, r3	->	sub.s	r50, r51, 0x1a
 700:	882b2cf7 8ab1beef 	sub.l	r50, r51, 0xdeadbeef
 708:	08301083 883b2cda 	sub2h.s	r1, r2, r3	->	sub2h.s	r50, r51, 0x1a
 710:	883b2cf7 8ab1beef 	sub2h.l	r50, r51, 0xdeadbeef
 718:	08501083 885b2cda 	subb.s	r1, r2, r3	->	subb.s	r50, r51, 0x1a
 720:	885b2cf7 8ab1beef 	subb.l	r50, r51, 0xdeadbeef
 728:	09801083 898b2cda 	subhlll.s	r1, r2, r3	->	subhlll.s	r50, r51, 0x1a
 730:	898b2cf7 8ab1beef 	subhlll.l	r50, r51, 0xdeadbeef
 738:	09901083 899b2cda 	subhllh.s	r1, r2, r3	->	subhllh.s	r50, r51, 0x1a
 740:	899b2cf7 8ab1beef 	subhllh.l	r50, r51, 0xdeadbeef
 748:	09a01083 89ab2cda 	subhlhl.s	r1, r2, r3	->	subhlhl.s	r50, r51, 0x1a
 750:	89ab2cf7 8ab1beef 	subhlhl.l	r50, r51, 0xdeadbeef
 758:	09b01083 89bb2cda 	subhlhh.s	r1, r2, r3	->	subhlhh.s	r50, r51, 0x1a
 760:	89bb2cf7 8ab1beef 	subhlhh.l	r50, r51, 0xdeadbeef
 768:	09c01083 89cb2cda 	subhhll.s	r1, r2, r3	->	subhhll.s	r50, r51, 0x1a
 770:	89cb2cf7 8ab1beef 	subhhll.l	r50, r51, 0xdeadbeef
 778:	09d01083 89db2cda 	subhhlh.s	r1, r2, r3	->	subhhlh.s	r50, r51, 0x1a
 780:	89db2cf7 8ab1beef 	subhhlh.l	r50, r51, 0xdeadbeef
 788:	09e01083 89eb2cda 	subhhhl.s	r1, r2, r3	->	subhhhl.s	r50, r51, 0x1a
 790:	89eb2cf7 8ab1beef 	subhhhl.l	r50, r51, 0xdeadbeef
 798:	09f01083 89fb2cda 	subhhhh.s	r1, r2, r3	->	subhhhh.s	r50, r51, 0x1a
 7a0:	89fb2cf7 8ab1beef 	subhhhh.l	r50, r51, 0xdeadbeef
 7a8:	00900001 00f00000 	trap.s	r1	||	nop	
 7b0:	0098000a 00f00000 	trap.s	0xa	||	nop	
 7b8:	03b01084 83b8108a 	xor.s	r1, r2, r4	->	xor.s	r1, r2, 0xa
 7c0:	83b810b7 8ab1f00d 	xor.l	r1, r2, 0xdeadf00d
 7c8:	02b01084 82b8110a 	xorfg	f1, f2, s	->	xorfg	f1, s, 0xa
 7d0:	00f00000 80f00000 	nop		->	nop	
 7d8:	00f00000 80f00000 	nop		->	nop	
 7e0:	00f00000 00f00000 	nop		||	nop	
 7e8:	80f00000 00f00000 	nop		<-	nop	
 7f0:	03901080 00f00000 	not	r1, r2	||	nop	
 7f8:	039020c0 80f00000 	not	r2, r3	->	nop	
