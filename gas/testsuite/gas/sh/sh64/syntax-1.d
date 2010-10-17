#as: --isa=shmedia --abi=64 --no-exp
#objdump: -d
#name: Minimum SH64 Syntax Support.

.*:     file format elf64-sh64.*

Disassembly of section .text:

0000000000000000 <.*>:
   0:	88100410 	ld.l	r1,4,r1
   4:	88100410 	ld.l	r1,4,r1
   8:	e8003a00 	pta/l	40 <.*>,tr0
   c:	e8003600 	pta/l	40 <.*>,tr0
  10:	e8003000 	pta/u	40 <.*>,tr0
  14:	e8002c00 	pta/u	40 <.*>,tr0
  18:	e8002a00 	pta/l	40 <.*>,tr0
  1c:	e8002600 	pta/l	40 <.*>,tr0
  20:	ec002000 	ptb/u	40 <.*>,tr0
  24:	ec001c00 	ptb/u	40 <.*>,tr0
  28:	ec001a00 	ptb/l	40 <.*>,tr0
  2c:	ec001600 	ptb/l	40 <.*>,tr0
  30:	e8001200 	pta/l	40 <.*>,tr0
  34:	e8000e00 	pta/l	40 <.*>,tr0
  38:	ec000a00 	ptb/l	40 <.*>,tr0
  3c:	ec000600 	ptb/l	40 <.*>,tr0
  40:	040983f0 	or	r0,r32,r63
  44:	240ffc00 	getcon	sr,r0
  48:	27fffc00 	getcon	usr,r0
  4c:	4405fc00 	gettr	tr0,r0
  50:	4475fc00 	gettr	tr7,r0
  54:	380003f0 	fmov.s	fr0,fr63
  58:	380103e0 	fmov.d	dr0,dr62
  5c:	140e0000 	ftrv.s	mtrx0,fv0,fv0
  60:	170ef3c0 	ftrv.s	mtrx48,fv60,fv60
  64:	240ffc00 	getcon	sr,r0
  68:	241ffc00 	getcon	ssr,r0
  6c:	242ffc00 	getcon	pssr,r0
  70:	244ffc00 	getcon	intevt,r0
  74:	245ffc00 	getcon	expevt,r0
  78:	246ffc00 	getcon	pexpevt,r0
  7c:	247ffc00 	getcon	tra,r0
  80:	248ffc00 	getcon	spc,r0
  84:	249ffc00 	getcon	pspc,r0
  88:	24affc00 	getcon	resvec,r0
  8c:	24bffc00 	getcon	vbr,r0
  90:	24dffc00 	getcon	tea,r0
  94:	250ffc00 	getcon	dcr,r0
  98:	251ffc00 	getcon	kcr0,r0
  9c:	252ffc00 	getcon	kcr1,r0
  a0:	27effc00 	getcon	ctc,r0
  a4:	27fffc00 	getcon	usr,r0

00000000000000a8 <.*>:
  a8:	e0 04       	mov	#4,r0
  aa:	00 09       	nop	

00000000000000ac <.*>:
  ac:	cc001000 	movi	4,r0

00000000000000b0 <.*>:
  b0:	50 02       	mov.l	@\(8,r0\),r0
  b2:	00 09       	nop	

00000000000000b4 <.*>:
  b4:	b0000400 	ld.uw	r0,2,r0
  b8:	84000400 	ld.w	r0,2,r0
  bc:	a4000400 	st.w	r0,2,r0
  c0:	88000400 	ld.l	r0,4,r0
  c4:	a8000400 	st.l	r0,4,r0
  c8:	94000400 	fld.s	r0,4,fr0
  cc:	b4000400 	fst.s	r0,4,fr0
  d0:	e8000600 	pta/l	d4 <.*>,tr0
  d4:	ec000a00 	ptb/l	dc <.*>,tr0
  d8:	8c000400 	ld.q	r0,8,r0
  dc:	ac000400 	st.q	r0,8,r0
  e0:	9c000400 	fld.d	r0,8,dr0
  e4:	bc000400 	fst.d	r0,8,dr0
  e8:	98000400 	fld.p	r0,8,fp0
  ec:	b8000400 	fst.p	r0,8,fp0
  f0:	e00407f0 	alloco	r0,32
  f4:	e00507f0 	icbi	r0,32
  f8:	e00907f0 	ocbi	r0,32
  fc:	e00807f0 	ocbp	r0,32
 100:	e00c07f0 	ocbwb	r0,32
 104:	e00107f0 	prefi	r0,32

0000000000000108 <.*>:
 108:	90 01       	mov.w	10e <.*>,r0	! 0x8101
 10a:	85 01       	mov.w	@\(2,r0\),r0
 10c:	c5 01       	mov.w	@\(2,gbr\),r0
 10e:	81 01       	mov.w	r0,@\(2,r0\)
 110:	c1 01       	mov.w	r0,@\(2,gbr\)
 112:	8b 01       	bf	118 <.*>
 114:	89 01       	bt	11a <.*>
 116:	a0 01       	bra	11c <.*>
 118:	b0 01       	bsr	11e <.*>
 11a:	d0 00       	mov.l	11c <.*>,r0	! 0x5001c601
 11c:	50 01       	mov.l	@\(4,r0\),r0
 11e:	c6 01       	mov.l	@\(4,gbr\),r0
 120:	c7 01       	mova	128 <.*>,r0
 122:	10 01       	mov.l	r0,@\(4,r0\)
 124:	c2 01       	mov.l	r0,@\(4,gbr\)
 126:	00 09       	nop	

0000000000000128 <.*>:
 128:	00000139 	.long 0x00000139
 12c:	0000013d 	.long 0x0000013d
 130:	00000138 	.long 0x00000138
 134:	00000138 	.long 0x00000138

0000000000000138 <.*>:
 138:	00 00       	.word 0x0000
 13a:	01 40       	.word 0x0140
 13c:	00 00       	.word 0x0000
 13e:	01 61       	.word 0x0161

0000000000000140 <.*>:
 140:	cc000000 	movi	0,r0
 144:	c8000000 	shori	0,r0
 148:	6bf10200 	ptabs/l	r0,tr0
 14c:	4401fd20 	blink	tr0,r18
 150:	cc000000 	movi	0,r0
 154:	c8000000 	shori	0,r0
 158:	6bf10200 	ptabs/l	r0,tr0
 15c:	4401fd20 	blink	tr0,r18
 160:	cfff7000 	movi	-36,r0
 164:	cfffe400 	movi	-7,r0
 168:	ebfffa00 	pta/l	160 <.*>,tr0

000000000000016c <.*>:
 16c:	0000016d 	.long 0x0000016d

0000000000000170 <.*>:
 170:	00000171 	.long 0x00000171
 174:	cfffd000 	movi	-12,r0
 178:	cfffc000 	movi	-16,r0

000000000000017c <.*>:
 17c:	c7 01       	mova	184 <.*>,r0
 17e:	60 12       	mov.l	@r1,r0
 180:	30 1c       	add	r1,r0
 182:	00 03       	bsrf	r0

0000000000000184 <.*>:
 184:	00 00       	.word 0x0000
 186:	00 05       	mov.w	r0,@\(r0,r0\)

0000000000000188 <.*>:
 188:	cc002400 	movi	9,r0
 18c:	cc001c00 	movi	7,r0
 190:	cc004000 	movi	16,r0
 194:	cc001000 	movi	4,r0
 198:	cffff800 	movi	-2,r0
 19c:	cc000400 	movi	1,r0
 1a0:	cc002400 	movi	9,r0
 1a4:	cc006000 	movi	24,r0
 1a8:	cc002000 	movi	8,r0
