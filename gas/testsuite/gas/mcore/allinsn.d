#as:
#objdump: -drz
#name: allinsn

.*: +file format .*

Disassembly of section \.text:

0+000 <abs>:
   0:	01e0      	abs	r0

0+002 <addc>:
   2:	0621      	addc	r1, r2

0+004 <addi>:
   4:	2003      	addi	r3, 1

0+006 <addu>:
   6:	1c54      	addu	r4, r5

0+008 <and>:
   8:	1676      	and	r6, r7

0+00a <andi>:
   a:	2e28      	andi	r8, 2

0+00c <andn>:
   c:	1fa9      	andn	r9, r10

0+00e <asr>:
   e:	1acb      	asr	r11, r12

0+010 <asrc>:
  10:	3a0d      	asrc	r13

0+012 <asri>:
  12:	3bfe      	asri	r14, 31

0+014 <bclri>:
  14:	300f      	bclri	r15, 0

0+016 <bf>:
  16:	eff4      	bf	0x0

0+018 <bgeni>:
  18:	3270      	bgeni	r0, 7

0+01a <BGENI>:
  1a:	3280      	bgeni	r0, 8

0+01c <BGENi>:
  1c:	33f0      	bgeni	r0, 31

0+01e <bgenr>:
  1e:	1321      	bgenr	r1, r2

0+020 <bkpt>:
  20:	0000      	bkpt

0+022 <bmaski>:
  22:	2c83      	bmaski	r3, 8

0+024 <BMASKI>:
  24:	2df3      	bmaski	r3, 31

0+026 <br>:
  26:	f7ff      	br	0x26

0+028 <brev>:
  28:	00f4      	brev	r4

0+02a <bseti>:
  2a:	35e5      	bseti	r5, 30

0+02c <bsr>:
  2c:	ffe9      	bsr	0x0

0+02e <bt>:
  2e:	e7e8      	bt	0x0

0+030 <btsti>:
  30:	37b6      	btsti	r6, 27

0+032 <clrc>:
  32:	0f00      	cmpne	r0, r0

0+034 <clrf>:
  34:	01d7      	clrf	r7

0+036 <clrt>:
  36:	01c8      	clrt	r8

0+038 <cmphs>:
  38:	0ca9      	cmphs	r9, r10

0+03a <cmplt>:
  3a:	0dcb      	cmplt	r11, r12

0+03c <cmplei>:
  3c:	22eb      	cmplti	r11, 15

0+03e <cmplti>:
  3e:	23fd      	cmplti	r13, 32

0+040 <cmpne>:
  40:	0ffe      	cmpne	r14, r15

0+042 <cmpnei>:
  42:	2a00      	cmpnei	r0, 0

0+044 <decf>:
  44:	0091      	decf	r1

0+046 <decgt>:
  46:	01a2      	decgt	r2

0+048 <declt>:
  48:	0183      	declt	r3

0+04a <decne>:
  4a:	01b4      	decne	r4

0+04c <dect>:
  4c:	0085      	dect	r5

0+04e <divs>:
  4e:	3216      	divs	r6, r1

0+050 <divu>:
  50:	2c18      	divu	r8, r1

0+052 <doze>:
  52:	0006      	doze

0+054 <ff1>:
  54:	00ea      	ff1	r10

0+056 <incf>:
  56:	00bb      	incf	r11

0+058 <inct>:
  58:	00ac      	inct	r12

0+05a <ixh>:
  5a:	1ded      	ixh	r13, r14

0+05c <ixw>:
  5c:	150f      	ixw	r15, r0

0+05e <jbf>:
  5e:	efd0      	bf	0x0

0+060 <jbr>:
  60:	f00e      	br	0x7e

0+062 <jbsr>:
  62:	7f0a      	jsri	0x0	// from address pool at 0x8c

0+064 <jbt>:
  64:	e00c      	bt	0x7e

0+066 <jmp>:
  66:	00c1      	jmp	r1

0+068 <jmpi>:
  68:	7009      	jmpi	0x0	// from address pool at 0x8c

0+06a <jsr>:
  6a:	00d2      	jsr	r2

0+06c <jsri>:
  6c:	7f08      	jsri	0x0	// from address pool at 0x8c

0+06e <ld\.b>:
  6e:	a304      	ldb	r3, \(r4, 0\)

0+070 <ld\.h>:
  70:	c516      	ldh	r5, \(r6, 2\)

0+072 <ld\.w>:
  72:	8718      	ld	r7, \(r8, 4\)

0+074 <ldb>:
  74:	a9fa      	ldb	r9, \(r10, 15\)

0+076 <ldh>:
  76:	cbfc      	ldh	r11, \(r12, 30\)

0+078 <ld>:
  78:	8d5e      	ld	r13, \(r14, 20\)

0+07a <ldw>:
  7a:	8dfe      	ld	r13, \(r14, 60\)

0+07c <ldm>:
  7c:	0062      	ldm	r2-r15, \(r0\)

0+07e <fooloop>:
  7e:	0041      	ldq	r4-r7, \(r1\)

0+080 <loopt>:
  80:	048e      	loopt	r8, 0x64

0+082 <LRW>:
  82:	7903      	lrw	r9, (0x86|0x0	// from address pool at 0x90)

0+084 <lrw>:
  84:	7904      	lrw	r9, 0x4321

0+086 <foolit>:
  86:	1234      	mov	r4, r3

0+088 <lsl>:
  88:	1bba      	lsl	r10, r11

0+08a <lslc>:
  8a:	3c0c      	lslc	r12
  8c:	0000      	bkpt
			8c: ADDR32	\.text
  8e:	0000      	bkpt
  90:	(0000      	bkpt|0086      	dect	r6)
			90: ADDR32	\.text(\+0x86)?
  92:	0000      	bkpt
  94:	4321      	\.short 0x4321
  96:	0000      	bkpt

0+098 <lsli>:
  98:	3dfd      	lsli	r13, 31

0+09a <lsr>:
  9a:	0bfe      	lsr	r14, r15

0+09c <lsrc>:
  9c:	3e00      	lsrc	r0

0+09e <lsri>:
  9e:	3e11      	lsri	r1, 1

0+0a0 <mclri>:
  a0:	3064      	bclri	r4, 6

0+0a2 <mfcr>:
  a2:	1002      	mfcr	r2, psr

0+0a4 <mov>:
  a4:	1243      	mov	r3, r4

0+0a6 <movf>:
  a6:	0a65      	movf	r5, r6

0+0a8 <movi>:
  a8:	67f7      	movi	r7, 127

0+0aa <movt>:
  aa:	0298      	movt	r8, r9

0+0ac <mtcr>:
  ac:	180a      	mtcr	r10, psr

0+0ae <mult>:
  ae:	03cb      	mult	r11, r12

0+0b0 <mvc>:
  b0:	002d      	mvc	r13

0+0b2 <mvcv>:
  b2:	003e      	mvcv	r14

0+0b4 <neg>:
  b4:	2802      	rsubi	r2, 0

0+0b6 <not>:
  b6:	01ff      	not	r15

0+0b8 <or>:
  b8:	1e10      	or	r0, r1

0+0ba <rfi>:
  ba:	0003      	rfi

0+0bc <rolc>:
  bc:	0666      	addc	r6, r6

0+0be <rori>:
  be:	39a9      	rotli	r9, 26

0+0c0 <rotlc>:
  c0:	0666      	addc	r6, r6

0+0c2 <rotli>:
  c2:	38a2      	rotli	r2, 10

0+0c4 <rotri>:
  c4:	39a9      	rotli	r9, 26

0+0c6 <rsub>:
  c6:	1443      	rsub	r3, r4

0+0c8 <rsubi>:
  c8:	2805      	rsubi	r5, 0

0+0ca <rte>:
  ca:	0002      	rte

0+0cc <rts>:
  cc:	00cf      	jmp	r15

0+0ce <setc>:
  ce:	0c00      	cmphs	r0, r0

0+0d0 <sextb>:
  d0:	0156      	sextb	r6

0+0d2 <sexth>:
  d2:	0177      	sexth	r7

0+0d4 <st\.b>:
  d4:	b809      	stb	r8, \(r9, 0\)

0+0d6 <st\.h>:
  d6:	da1b      	sth	r10, \(r11, 2\)

0+0d8 <st\.w>:
  d8:	9c1d      	st	r12, \(r13, 4\)

0+0da <stb>:
  da:	beff      	stb	r14, \(r15, 15\)

0+0dc <sth>:
  dc:	d0f1      	sth	r0, \(r1, 30\)

0+0de <stw>:
  de:	92f3      	st	r2, \(r3, 60\)

0+0e0 <st>:
  e0:	9405      	st	r4, \(r5, 0\)

0+0e2 <stm>:
  e2:	007e      	stm	r14-r15, \(r0\)

0+0e4 <stop>:
  e4:	0004      	stop

0+0e6 <stq>:
  e6:	0051      	stq	r4-r7, \(r1\)

0+0e8 <subc>:
  e8:	07d7      	subc	r7, r13

0+0ea <subi>:
  ea:	25fe      	subi	r14, 32

0+0ec <subu>:
  ec:	0539      	subu	r9, r3

0+0ee <sync>:
  ee:	0001      	sync

0+0f0 <tstlt>:
  f0:	37f5      	btsti	r5, 31

0+0f2 <tstne>:
  f2:	2a07      	cmpnei	r7, 0

0+0f4 <trap>:
  f4:	000a      	trap	2

0+0f6 <tst>:
  f6:	0eee      	tst	r14, r14

0+0f8 <tstnbz>:
  f8:	0192      	tstnbz	r2

0+0fa <wait>:
  fa:	0005      	wait

0+0fc <xor>:
  fc:	170f      	xor	r15, r0

0+0fe <xsr>:
  fe:	380b      	xsr	r11

0+0100 <xtrb0>:
 100:	0131      	xtrb0	r1, r1

0+0102 <xtrb1>:
 102:	0122      	xtrb1	r1, r2

0+0104 <xtrb2>:
 104:	0110      	xtrb2	r1, r0

0+0106 <xtrb3>:
 106:	010d      	xtrb3	r1, r13

0+0108 <zextb>:
 108:	0148      	zextb	r8

0+010a <zexth>:
 10a:	0164      	zexth	r4
 10c:	0f00      	cmpne	r0, r0
 10e:	0f00      	cmpne	r0, r0
