#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	f4 01 70 02 	add         r0,r1,r2
   4:	f4 22 72 03 	add.ci      r1,r2,r3
   8:	f4 43 71 04 	add.co      r2,r3,r4
   c:	f4 64 73 05 	add.cio     r3,r4,r5
  10:	70 85 00 00 	add         r4,r5,0
  14:	70 85 10 00 	add         r4,r5,0x1000
  18:	f4 01 60 02 	addu        r0,r1,r2
  1c:	f4 22 62 03 	addu.ci     r1,r2,r3
  20:	f4 43 61 04 	addu.co     r2,r3,r4
  24:	f4 64 63 05 	addu.cio    r3,r4,r5
  28:	60 85 00 00 	addu        r4,r5,0
  2c:	60 85 10 00 	addu        r4,r5,0x1000
  30:	f4 01 40 02 	and         r0,r1,r2
  34:	f4 22 44 03 	and.c       r1,r2,r3
  38:	40 43 00 00 	and         r2,r3,0
  3c:	40 43 10 00 	and         r2,r3,0x1000
  40:	44 43 00 00 	and.u       r2,r3,0
  44:	44 43 10 00 	and.u       r2,r3,0x1000
  48:	d0 01 00 00 	bb0         0,r1,48 <.text\+0x48>
			4a: PCR16L	\*ABS\*
  4c:	d0 01 ff fd 	bb0         0,r1,40 <.text\+0x40>
			4e: PCR16L	\*ABS\*
  50:	d0 01 00 02 	bb0         0,r1,58 <.text\+0x58>
			52: PCR16L	\*ABS\*
  54:	d3 e1 00 00 	bb0         0x1f,r1,54 <.text\+0x54>
			56: PCR16L	\*ABS\*
  58:	d3 e1 ff fd 	bb0         0x1f,r1,4c <.text\+0x4c>
			5a: PCR16L	\*ABS\*
  5c:	d3 e1 00 02 	bb0         0x1f,r1,64 <.text\+0x64>
			5e: PCR16L	\*ABS\*
  60:	d4 01 00 00 	bb0.n       0,r1,60 <.text\+0x60>
			62: PCR16L	\*ABS\*
  64:	d8 01 00 00 	bb1         0,r1,64 <.text\+0x64>
			66: PCR16L	\*ABS\*
  68:	d8 01 ff fd 	bb1         0,r1,5c <.text\+0x5c>
			6a: PCR16L	\*ABS\*
  6c:	d8 01 00 02 	bb1         0,r1,74 <.text\+0x74>
			6e: PCR16L	\*ABS\*
  70:	db e1 00 00 	bb1         0x1f,r1,70 <.text\+0x70>
			72: PCR16L	\*ABS\*
  74:	db e1 ff fd 	bb1         0x1f,r1,68 <.text\+0x68>
			76: PCR16L	\*ABS\*
  78:	db e1 00 02 	bb1         0x1f,r1,80 <.text\+0x80>
			7a: PCR16L	\*ABS\*
  7c:	dc 01 00 00 	bb1.n       0,r1,7c <.text\+0x7c>
			7e: PCR16L	\*ABS\*
  80:	e8 41 00 00 	bcnd        eq0,r1,80 <.text\+0x80>
			82: PCR16L	\*ABS\*
  84:	e8 41 00 02 	bcnd        eq0,r1,8c <.text\+0x8c>
			86: PCR16L	\*ABS\*
  88:	e8 41 ff fd 	bcnd        eq0,r1,7c <.text\+0x7c>
			8a: PCR16L	\*ABS\*
  8c:	ec 41 00 00 	bcnd.n      eq0,r1,8c <.text\+0x8c>
			8e: PCR16L	\*ABS\*
  90:	ec 41 00 02 	bcnd.n      eq0,r1,98 <.text\+0x98>
			92: PCR16L	\*ABS\*
  94:	ec 41 ff fd 	bcnd.n      eq0,r1,88 <.text\+0x88>
			96: PCR16L	\*ABS\*
  98:	e9 a1 00 00 	bcnd        ne0,r1,98 <.text\+0x98>
			9a: PCR16L	\*ABS\*
  9c:	e9 a1 00 02 	bcnd        ne0,r1,a4 <.text\+0xa4>
			9e: PCR16L	\*ABS\*
  a0:	e9 a1 ff fd 	bcnd        ne0,r1,94 <.text\+0x94>
			a2: PCR16L	\*ABS\*
  a4:	ed a1 00 00 	bcnd.n      ne0,r1,a4 <.text\+0xa4>
			a6: PCR16L	\*ABS\*
  a8:	ed a1 00 02 	bcnd.n      ne0,r1,b0 <.text\+0xb0>
			aa: PCR16L	\*ABS\*
  ac:	ed a1 ff fd 	bcnd.n      ne0,r1,a0 <.text\+0xa0>
			ae: PCR16L	\*ABS\*
  b0:	e8 21 00 00 	bcnd        gt0,r1,b0 <.text\+0xb0>
			b2: PCR16L	\*ABS\*
  b4:	e8 21 00 02 	bcnd        gt0,r1,bc <.text\+0xbc>
			b6: PCR16L	\*ABS\*
  b8:	e8 21 ff fd 	bcnd        gt0,r1,ac <.text\+0xac>
			ba: PCR16L	\*ABS\*
  bc:	ec 21 00 00 	bcnd.n      gt0,r1,bc <.text\+0xbc>
			be: PCR16L	\*ABS\*
  c0:	ec 21 00 02 	bcnd.n      gt0,r1,c8 <.text\+0xc8>
			c2: PCR16L	\*ABS\*
  c4:	ec 21 ff fd 	bcnd.n      gt0,r1,b8 <.text\+0xb8>
			c6: PCR16L	\*ABS\*
  c8:	e9 81 00 00 	bcnd        lt0,r1,c8 <.text\+0xc8>
			ca: PCR16L	\*ABS\*
  cc:	e9 81 00 02 	bcnd        lt0,r1,d4 <.text\+0xd4>
			ce: PCR16L	\*ABS\*
  d0:	e9 81 ff fd 	bcnd        lt0,r1,c4 <.text\+0xc4>
			d2: PCR16L	\*ABS\*
  d4:	ed 81 00 00 	bcnd.n      lt0,r1,d4 <.text\+0xd4>
			d6: PCR16L	\*ABS\*
  d8:	ed 81 00 02 	bcnd.n      lt0,r1,e0 <.text\+0xe0>
			da: PCR16L	\*ABS\*
  dc:	ed 81 ff fd 	bcnd.n      lt0,r1,d0 <.text\+0xd0>
			de: PCR16L	\*ABS\*
  e0:	e8 61 00 00 	bcnd        ge0,r1,e0 <.text\+0xe0>
			e2: PCR16L	\*ABS\*
  e4:	e8 61 00 02 	bcnd        ge0,r1,ec <.text\+0xec>
			e6: PCR16L	\*ABS\*
  e8:	e8 61 ff fd 	bcnd        ge0,r1,dc <.text\+0xdc>
			ea: PCR16L	\*ABS\*
  ec:	ec 61 00 00 	bcnd.n      ge0,r1,ec <.text\+0xec>
			ee: PCR16L	\*ABS\*
  f0:	ec 61 00 02 	bcnd.n      ge0,r1,f8 <.text\+0xf8>
			f2: PCR16L	\*ABS\*
  f4:	ec 61 ff fd 	bcnd.n      ge0,r1,e8 <.text\+0xe8>
			f6: PCR16L	\*ABS\*
  f8:	e9 c1 00 00 	bcnd        le0,r1,f8 <.text\+0xf8>
			fa: PCR16L	\*ABS\*
  fc:	e9 c1 00 02 	bcnd        le0,r1,104 <.text\+0x104>
			fe: PCR16L	\*ABS\*
 100:	e9 c1 ff fd 	bcnd        le0,r1,f4 <.text\+0xf4>
			102: PCR16L	\*ABS\*
 104:	ed c1 00 00 	bcnd.n      le0,r1,104 <.text\+0x104>
			106: PCR16L	\*ABS\*
 108:	ed c1 00 02 	bcnd.n      le0,r1,110 <.text\+0x110>
			10a: PCR16L	\*ABS\*
 10c:	ed c1 ff fd 	bcnd.n      le0,r1,100 <.text\+0x100>
			10e: PCR16L	\*ABS\*
 110:	e8 61 00 00 	bcnd        ge0,r1,110 <.text\+0x110>
			112: PCR16L	\*ABS\*
 114:	e8 61 00 02 	bcnd        ge0,r1,11c <.text\+0x11c>
			116: PCR16L	\*ABS\*
 118:	e8 61 ff fd 	bcnd        ge0,r1,10c <.text\+0x10c>
			11a: PCR16L	\*ABS\*
 11c:	ec 61 00 00 	bcnd.n      ge0,r1,11c <.text\+0x11c>
			11e: PCR16L	\*ABS\*
 120:	ec 61 00 02 	bcnd.n      ge0,r1,128 <.text\+0x128>
			122: PCR16L	\*ABS\*
 124:	ec 61 ff fd 	bcnd.n      ge0,r1,118 <.text\+0x118>
			126: PCR16L	\*ABS\*
 128:	c0 00 00 00 	br          128 <.text\+0x128>
			128: PCR26L	\*ABS\*
 12c:	c3 ff ff fd 	br          120 <.text\+0x120>
			12c: PCR26L	\*ABS\*
 130:	c0 00 00 02 	br          138 <.text\+0x138>
			130: PCR26L	\*ABS\*
 134:	c4 00 00 00 	br.n        134 <.text\+0x134>
			134: PCR26L	\*ABS\*
 138:	c7 ff ff fd 	br.n        12c <.text\+0x12c>
			138: PCR26L	\*ABS\*
 13c:	c4 00 00 02 	br.n        144 <.text\+0x144>
			13c: PCR26L	\*ABS\*
 140:	c8 00 00 00 	bsr         140 <.text\+0x140>
			140: PCR26L	\*ABS\*
 144:	cb ff ff fd 	bsr         138 <.text\+0x138>
			144: PCR26L	\*ABS\*
 148:	c8 00 00 02 	bsr         150 <.text\+0x150>
			148: PCR26L	\*ABS\*
 14c:	cc 00 00 00 	bsr.n       14c <.text\+0x14c>
			14c: PCR26L	\*ABS\*
 150:	cf ff ff fd 	bsr.n       144 <.text\+0x144>
			150: PCR26L	\*ABS\*
 154:	cc 00 00 02 	bsr.n       15c <.text\+0x15c>
			154: PCR26L	\*ABS\*
 158:	f0 22 80 af 	clr         r1,r2,5<15>
 15c:	f4 22 80 03 	clr         r1,r2,r3
 160:	f0 22 80 06 	clr         r1,r2,0<6>
 164:	f0 22 80 06 	clr         r1,r2,0<6>
 168:	f4 01 7c 02 	cmp         r0,r1,r2
 16c:	7c 02 00 00 	cmp         r0,r2,0
 170:	7c 02 10 00 	cmp         r0,r2,0x1000
 174:	f4 01 78 02 	divs        r0,r1,r2
 178:	78 01 00 00 	divs        r0,r1,0
 17c:	78 01 10 00 	divs        r0,r1,0x1000
 180:	f4 01 68 02 	divu        r0,r1,r2
 184:	68 01 00 00 	divu        r0,r1,0
 188:	68 01 00 0a 	divu        r0,r1,0x0a
 18c:	f0 01 91 45 	ext         r0,r1,10<5>
 190:	f4 22 90 03 	ext         r1,r2,r3
 194:	f0 43 90 06 	ext         r2,r3,0<6>
 198:	f0 43 90 06 	ext         r2,r3,0<6>
 19c:	f0 01 99 45 	extu        r0,r1,10<5>
 1a0:	f4 22 98 03 	extu        r1,r2,r3
 1a4:	f0 22 98 06 	extu        r1,r2,0<6>
 1a8:	f0 22 98 06 	extu        r1,r2,0<6>
 1ac:	84 01 28 02 	fadd.sss    r0,r1,r2
 1b0:	84 01 28 82 	fadd.ssd    r0,r1,r2
 1b4:	84 01 2a 02 	fadd.sds    r0,r1,r2
 1b8:	84 01 2a 82 	fadd.sdd    r0,r1,r2
 1bc:	84 01 28 22 	fadd.dss    r0,r1,r2
 1c0:	84 01 28 a2 	fadd.dsd    r0,r1,r2
 1c4:	84 01 2a 22 	fadd.dds    r0,r1,r2
 1c8:	84 01 2a a2 	fadd.ddd    r0,r1,r2
 1cc:	84 01 38 02 	fcmp.ss     r0,r1,r2
 1d0:	84 01 38 82 	fcmp.sd     r0,r1,r2
 1d4:	84 01 3a 02 	fcmp.ds     r0,r1,r2
 1d8:	84 01 3a 82 	fcmp.dd     r0,r1,r2
 1dc:	84 01 70 02 	fdiv.sss    r0,r1,r2
 1e0:	84 01 70 82 	fdiv.ssd    r0,r1,r2
 1e4:	84 01 72 02 	fdiv.sds    r0,r1,r2
 1e8:	84 01 72 82 	fdiv.sdd    r0,r1,r2
 1ec:	84 01 70 22 	fdiv.dss    r0,r1,r2
 1f0:	84 01 70 a2 	fdiv.dsd    r0,r1,r2
 1f4:	84 01 72 22 	fdiv.dds    r0,r1,r2
 1f8:	84 01 72 a2 	fdiv.ddd    r0,r1,r2
 1fc:	f4 20 ec 07 	ff0         r1,r7
 200:	f4 60 e8 08 	ff1         r3,r8
 204:	80 00 4e 40 	fldcr       r0,fcr50
 208:	84 00 20 03 	flt.s       r0,r3
 20c:	84 00 20 2a 	flt.d       r0,r10
 210:	84 01 00 02 	fmul.sss    r0,r1,r2
 214:	84 01 00 82 	fmul.ssd    r0,r1,r2
 218:	84 01 02 02 	fmul.sds    r0,r1,r2
 21c:	84 01 02 82 	fmul.sdd    r0,r1,r2
 220:	84 01 00 22 	fmul.dss    r0,r1,r2
 224:	84 01 00 a2 	fmul.dsd    r0,r1,r2
 228:	84 01 02 22 	fmul.dds    r0,r1,r2
 22c:	84 01 02 a2 	fmul.ddd    r0,r1,r2
 230:	80 00 8e 40 	fstcr       r0,fcr50
 234:	84 01 30 02 	fsub.sss    r0,r1,r2
 238:	84 01 30 82 	fsub.ssd    r0,r1,r2
 23c:	84 01 32 02 	fsub.sds    r0,r1,r2
 240:	84 01 32 82 	fsub.sdd    r0,r1,r2
 244:	84 01 30 22 	fsub.dss    r0,r1,r2
 248:	84 01 30 a2 	fsub.dsd    r0,r1,r2
 24c:	84 01 32 22 	fsub.dds    r0,r1,r2
 250:	84 01 32 a2 	fsub.ddd    r0,r1,r2
 254:	80 01 ce 41 	fxcr        r0,r1,fcr50
 258:	84 00 48 01 	int.s       r0,r1
 25c:	85 40 48 82 	int.d       r10,r2
 260:	f4 00 c0 00 	jmp         r0
 264:	f4 00 c4 0a 	jmp.n       r10
 268:	f4 00 c8 0a 	jsr         r10
 26c:	f4 00 cc 0d 	jsr.n       r13
 270:	1c 01 00 00 	ld.b        r0,r1,0
 274:	1c 01 10 00 	ld.b        r0,r1,0x1000
 278:	0c 01 00 00 	ld.bu       r0,r1,0
 27c:	0c 01 10 00 	ld.bu       r0,r1,0x1000
 280:	18 01 00 00 	ld.h        r0,r1,0
 284:	18 01 10 00 	ld.h        r0,r1,0x1000
 288:	08 01 00 00 	ld.hu       r0,r1,0
 28c:	08 01 10 00 	ld.hu       r0,r1,0x1000
 290:	14 01 00 00 	ld          r0,r1,0
 294:	14 01 10 00 	ld          r0,r1,0x1000
 298:	10 01 00 00 	ld.d        r0,r1,0
 29c:	10 01 10 00 	ld.d        r0,r1,0x1000
 2a0:	f4 01 1c 02 	ld.b        r0,r1,r2
 2a4:	f4 22 0c 03 	ld.bu       r1,r2,r3
 2a8:	f4 43 18 04 	ld.h        r2,r3,r4
 2ac:	f4 64 08 05 	ld.hu       r3,r4,r5
 2b0:	f4 85 14 06 	ld          r4,r5,r6
 2b4:	f4 a6 10 07 	ld.d        r5,r6,r7
 2b8:	f4 c7 1d 08 	word	f4c71d08
 2bc:	f4 e8 0d 09 	word	f4e80d09
 2c0:	f5 09 19 01 	word	f5091901
 2c4:	f5 21 09 02 	word	f5210902
 2c8:	f4 22 15 03 	ld.usr      r1,r2,r3
 2cc:	f4 43 11 04 	word	f4431104
 2d0:	f4 01 1e 02 	word	f4011e02
 2d4:	f4 22 0e 03 	word	f4220e03
 2d8:	f4 43 1a 04 	ld.h        r2,r3\[r4\]
 2dc:	f4 64 0a 05 	ld.hu       r3,r4\[r5\]
 2e0:	f4 85 16 06 	ld          r4,r5\[r6\]
 2e4:	f4 a6 12 07 	ld.d        r5,r6\[r7\]
 2e8:	f4 c7 1f 08 	word	f4c71f08
 2ec:	f4 e8 0f 09 	word	f4e80f09
 2f0:	f5 09 1b 01 	word	f5091b01
 2f4:	f5 21 0b 02 	word	f5210b02
 2f8:	f4 22 17 03 	ld.usr      r1,r2\[r3\]
 2fc:	f4 43 13 04 	word	f4431304
 300:	f4 01 3a 02 	lda.h       r0,r1\[r2\]
 304:	f4 22 36 03 	lda         r1,r2\[r3\]
 308:	f4 43 32 04 	lda.d       r2,r3\[r4\]
 30c:	80 00 41 40 	ldcr        r0,cr10
 310:	f0 01 a1 45 	mak         r0,r1,10<5>
 314:	f4 01 a0 02 	mak         r0,r1,r2
 318:	f0 01 a0 06 	mak         r0,r1,0<6>
 31c:	f0 01 a0 06 	mak         r0,r1,0<6>
 320:	48 01 00 00 	mask        r0,r1,0
 324:	48 01 10 00 	mask        r0,r1,0x1000
 328:	4c 01 00 00 	mask.u      r0,r1,0
 32c:	4c 01 10 00 	mask.u      r0,r1,0x1000
 330:	f4 01 6c 02 	mulu        r0,r1,r2
 334:	6c 01 00 00 	mulu        r0,r1,0
 338:	6c 01 10 00 	mulu        r0,r1,0x1000
 33c:	84 00 50 0a 	nint.s      r0,r10
 340:	85 40 50 8c 	nint.d      r10,r12
 344:	f4 01 58 02 	or          r0,r1,r2
 348:	f4 27 5c 0a 	or.c        r1,r7,r10
 34c:	58 04 00 00 	or          r0,r4,0
 350:	58 04 10 00 	or          r0,r4,0x1000
 354:	5c 01 00 00 	or.u        r0,r1,0
 358:	5c 44 10 00 	or.u        r2,r4,0x1000
 35c:	f0 01 a8 05 	rot         r0,r1,0<5>
 360:	f4 44 a8 06 	rot         r2,r4,r6
 364:	f4 00 fc 00 	rte         
 368:	f0 01 89 45 	set         r0,r1,10<5>
 36c:	f4 44 88 06 	set         r2,r4,r6
 370:	f0 67 88 06 	set         r3,r7,0<6>
 374:	f0 67 88 06 	set         r3,r7,0<6>
 378:	2c 01 00 00 	st.b        r0,r1,0
 37c:	2c 01 10 00 	st.b        r0,r1,0x1000
 380:	28 01 00 00 	st.h        r0,r1,0
 384:	28 01 10 00 	st.h        r0,r1,0x1000
 388:	24 01 00 00 	st          r0,r1,0
 38c:	24 01 10 00 	st          r0,r1,0x1000
 390:	20 01 00 00 	st.d        r0,r1,0
 394:	20 01 10 00 	st.d        r0,r1,0x1000
 398:	f4 01 2c 02 	st.b        r0,r1,r2
 39c:	f4 43 28 04 	st.h        r2,r3,r4
 3a0:	f4 85 24 06 	st          r4,r5,r6
 3a4:	f4 a6 20 07 	st.d        r5,r6,r7
 3a8:	f4 c7 2d 08 	word	f4c72d08
 3ac:	f5 09 29 01 	word	f5092901
 3b0:	f4 22 25 03 	st.usr      r1,r2,r3
 3b4:	f4 43 21 04 	word	f4432104
 3b8:	f4 01 2e 02 	word	f4012e02
 3bc:	f4 43 2a 04 	st.h        r2,r3\[r4\]
 3c0:	f4 85 26 06 	st          r4,r5\[r6\]
 3c4:	f4 a6 22 07 	st.d        r5,r6\[r7\]
 3c8:	f4 c7 2f 08 	word	f4c72f08
 3cc:	f5 09 2b 01 	word	f5092b01
 3d0:	f4 22 27 03 	st.usr      r1,r2\[r3\]
 3d4:	f4 43 23 04 	word	f4432304
 3d8:	80 00 81 40 	stcr        r0,cr10
 3dc:	f4 01 74 02 	sub         r0,r1,r2
 3e0:	f4 22 76 03 	sub.ci      r1,r2,r3
 3e4:	f4 43 75 04 	sub.co      r2,r3,r4
 3e8:	f4 64 77 05 	sub.cio     r3,r4,r5
 3ec:	74 85 00 00 	sub         r4,r5,0
 3f0:	74 85 10 00 	sub         r4,r5,0x1000
 3f4:	f4 01 64 02 	subu        r0,r1,r2
 3f8:	f4 22 66 03 	subu.ci     r1,r2,r3
 3fc:	f4 64 65 05 	subu.co     r3,r4,r5
 400:	f4 85 67 06 	subu.cio    r4,r5,r6
 404:	64 a6 00 00 	subu        r5,r6,0
 408:	64 a6 10 00 	subu        r5,r6,0x1000
 40c:	f0 0a d0 0a 	tb0         0,r10,0x0a
 410:	f3 eb d0 0a 	tb0         0x1f,r11,0x0a
 414:	f0 0a d8 0a 	tb1         0,r10,0x0a
 418:	f3 eb d8 0a 	tb1         0x1f,r11,0x0a
 41c:	f4 00 f8 01 	tbnd        r0,r1
 420:	f8 07 00 00 	tbnd        r7,0
 424:	f8 07 10 00 	tbnd        r7,0x1000
 428:	f0 4a e8 0c 	tcnd        eq0,r10,0x0c
 42c:	f1 a9 e8 0c 	tcnd        ne0,r9,0x0c
 430:	f0 28 e8 07 	tcnd        gt0,r8,0x07
 434:	f1 87 e8 01 	tcnd        lt0,r7,0x01
 438:	f0 66 e8 23 	tcnd        ge0,r6,0x23
 43c:	f1 c5 e8 21 	tcnd        le0,r5,0x21
 440:	f1 44 e8 0c 	tcnd        a,r4,0x0c
 444:	84 00 58 01 	trnc.s      r0,r1
 448:	84 20 58 83 	trnc.d      r1,r3
 44c:	80 03 c1 43 	xcr         r0,r3,cr10
 450:	f4 01 00 02 	xmem.bu     r0,r1,r2
 454:	f4 22 04 03 	xmem        r1,r2,r3
 458:	f4 85 01 06 	word	f4850106
 45c:	f4 a6 05 07 	xmem.usr    r5,r6,r7
 460:	f4 43 02 04 	word	f4430204
 464:	f4 64 06 05 	xmem        r3,r4\[r5\]
 468:	f4 85 03 09 	word	f4850309
 46c:	f4 a6 07 0a 	xmem.usr    r5,r6\[r10\]
 470:	f4 01 50 02 	xor         r0,r1,r2
 474:	f4 22 54 03 	xor.c       r1,r2,r3
 478:	50 43 00 00 	xor         r2,r3,0
 47c:	50 44 10 00 	xor         r2,r4,0x1000
 480:	54 22 00 00 	xor.u       r1,r2,0
 484:	54 43 10 00 	xor.u       r2,r3,0x1000
 488:	f4 00 58 00 	or          r0,r0,r0
 48c:	f4 00 58 00 	or          r0,r0,r0
