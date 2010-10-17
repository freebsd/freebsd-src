# objdump: -dr
# source: list-insns.s
# as: -no-expand
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	00000003 	trap 0,0,3
   4:	00030405 	trap 3,4,5
   8:	010c17f1 	fcmp \$12,\$23,\$241
   c:	08700129 	flot \$112,ROUND_OFF,\$41
  10:	0970048d 	flot \$112,ROUND_NEAR,141
  14:	08bf00f2 	flot \$191,\$242
  18:	09c3002a 	flot \$195,42
  1c:	027acb04 	fun \$122,\$203,\$4
  20:	03661e28 	feql \$102,\$30,\$40
  24:	0a66000e 	flotu \$102,\$14
  28:	0a84020e 	flotu \$132,ROUND_UP,\$14
  2c:	0a660368 	flotu \$102,ROUND_DOWN,\$104
  30:	0aac048c 	flotu \$172,ROUND_NEAR,\$140
  34:	0a010186 	flotu \$1,ROUND_OFF,\$134
  38:	0470df29 	fadd \$112,\$223,\$41
  3c:	05700129 	fix \$112,ROUND_OFF,\$41
  40:	050b008d 	fix \$11,\$141
  44:	0c700129 	sflot \$112,ROUND_OFF,\$41
  48:	0d70048d 	sflot \$112,ROUND_NEAR,141
  4c:	0670df29 	fsub \$112,\$223,\$41
  50:	0766000e 	fixu \$102,\$14
  54:	0784020e 	fixu \$132,ROUND_UP,\$14
  58:	0e0b008d 	sflotu \$11,\$141
  5c:	0f70008d 	sflotu \$112,141
  60:	0f70048d 	sflotu \$112,ROUND_NEAR,141
  64:	0e700129 	sflotu \$112,ROUND_OFF,\$41
  68:	10661e28 	fmul \$102,\$30,\$40
  6c:	110cdf01 	fcmpe \$12,\$223,\$1
  70:	197acb2c 	mul \$122,\$203,44
  74:	18661e28 	mul \$102,\$30,\$40
  78:	130cdf01 	feqle \$12,\$223,\$1
  7c:	120cdf0b 	fune \$12,\$223,\$11
  80:	1b7ad52c 	mulu \$122,\$213,44
  84:	1a841e28 	mulu \$132,\$30,\$40
  88:	140cdf0b 	fdiv \$12,\$223,\$11
  8c:	1584020e 	fsqrt \$132,ROUND_UP,\$14
  90:	150b008d 	fsqrt \$11,\$141
  94:	1d7ad52c 	div \$122,\$213,44
  98:	1c841e28 	div \$132,\$30,\$40
  9c:	160cdf0b 	frem \$12,\$223,\$11
  a0:	1784020e 	fint \$132,ROUND_UP,\$14
  a4:	170b008d 	fint \$11,\$141
  a8:	1e0cdf01 	divu \$12,\$223,\$1
  ac:	1f7acbff 	divu \$122,\$203,255
  b0:	200cdf01 	add \$12,\$223,\$1
  b4:	217acbff 	add \$122,\$203,255
  b8:	280cdf0b 	2addu \$12,\$223,\$11
  bc:	297acb00 	2addu \$122,\$203,0
  c0:	237acbff 	addu \$122,\$203,255
  c4:	220cdf0b 	addu \$12,\$223,\$11
  c8:	237acbff 	addu \$122,\$203,255
  cc:	220cdf0b 	addu \$12,\$223,\$11
  d0:	2b7acbcd 	4addu \$122,\$203,205
  d4:	2a0cdf6f 	4addu \$12,\$223,\$111
  d8:	240cdf0b 	sub \$12,\$223,\$11
  dc:	257acbcd 	sub \$122,\$203,205
  e0:	2c0cdf0b 	8addu \$12,\$223,\$11
  e4:	2d7acbcd 	8addu \$122,\$203,205
  e8:	2602df0b 	subu \$2,\$223,\$11
  ec:	270c14cd 	subu \$12,\$20,205
  f0:	2e02df0b 	16addu \$2,\$223,\$11
  f4:	2f0c14cd 	16addu \$12,\$20,205
  f8:	3002df0b 	cmp \$2,\$223,\$11
  fc:	310c14cd 	cmp \$12,\$20,205
 100:	3802df0b 	sl \$2,\$223,\$11
 104:	390c14cd 	sl \$12,\$20,205
 108:	3202df0b 	cmpu \$2,\$223,\$11
 10c:	330c14cd 	cmpu \$12,\$20,205
 110:	3a02df0b 	slu \$2,\$223,\$11
 114:	3b0c14cd 	slu \$12,\$20,205
 118:	3402170b 	neg \$2,23,\$11
 11c:	350c00cd 	neg \$12,0,205
 120:	35c00acd 	neg \$192,10,205
 124:	3d0c14cd 	sr \$12,\$20,205
 128:	3c02df0b 	sr \$2,\$223,\$11
 12c:	3602170b 	negu \$2,23,\$11
 130:	370c00cd 	negu \$12,0,205
 134:	3f0c14cd 	sru \$12,\$20,205
 138:	3e02df0b 	sru \$2,\$223,\$11
 13c:	40020001 	bn \$2,140 <Main\+0x140>
 140:	4102ffff 	bn \$2,13c <Main\+0x13c>
 144:	4902ffff 	bnn \$2,140 <Main\+0x140>
 148:	4902ffff 	bnn \$2,144 <Main\+0x144>
 14c:	42ff0001 	bz \$255,150 <Main\+0x150>
 150:	43ffffff 	bz \$255,14c <Main\+0x14c>
 154:	4aff0001 	bnz \$255,158 <Main\+0x158>
 158:	4bffffff 	bnz \$255,154 <Main\+0x154>
 15c:	44190001 	bp \$25,160 <Main\+0x160>
 160:	4519ffff 	bp \$25,15c <Main\+0x15c>
 164:	4c190001 	bnp \$25,168 <Main\+0x168>
 168:	4d19ffff 	bnp \$25,164 <Main\+0x164>
 16c:	46190001 	bod \$25,170 <Main\+0x170>
 170:	4719ffff 	bod \$25,16c <Main\+0x16c>
 174:	4e190001 	bev \$25,178 <Main\+0x178>
 178:	4f19ffff 	bev \$25,174 <Main\+0x174>
 17c:	50020001 	pbn \$2,180 <Main\+0x180>
 180:	5102ffff 	pbn \$2,17c <Main\+0x17c>
 184:	58020001 	pbnn \$2,188 <Main\+0x188>
 188:	5902ffff 	pbnn \$2,184 <Main\+0x184>
 18c:	520c0001 	pbz \$12,190 <Main\+0x190>
 190:	5316ffff 	pbz \$22,18c <Main\+0x18c>
 194:	5a200001 	pbnz \$32,198 <Main\+0x198>
 198:	5b34ffff 	pbnz \$52,194 <Main\+0x194>
 19c:	56190001 	pbod \$25,1a0 <Main\+0x1a0>
 1a0:	5719ffff 	pbod \$25,19c <Main\+0x19c>
 1a4:	5e190001 	pbev \$25,1a8 <Main\+0x1a8>
 1a8:	5f19ffff 	pbev \$25,1a4 <Main\+0x1a4>
 1ac:	6002df0b 	csn \$2,\$223,\$11
 1b0:	610c14cd 	csn \$12,\$20,205
 1b4:	6802df0b 	csnn \$2,\$223,\$11
 1b8:	690c14cd 	csnn \$12,\$20,205
 1bc:	6202cb0b 	csz \$2,\$203,\$11
 1c0:	630cc8cd 	csz \$12,\$200,205
 1c4:	6a02cb0b 	csnz \$2,\$203,\$11
 1c8:	6b0cc8cd 	csnz \$12,\$200,205
 1cc:	6402cb0b 	csp \$2,\$203,\$11
 1d0:	650cc8cd 	csp \$12,\$200,205
 1d4:	6c02cb0b 	csnp \$2,\$203,\$11
 1d8:	6d0cc8cd 	csnp \$12,\$200,205
 1dc:	6602cb0b 	csod \$2,\$203,\$11
 1e0:	670cc8cd 	csod \$12,\$200,205
 1e4:	6e02cb0b 	csev \$2,\$203,\$11
 1e8:	6f0cc8cd 	csev \$12,\$200,205
 1ec:	7002df0b 	zsn \$2,\$223,\$11
 1f0:	710c14cd 	zsn \$12,\$20,205
 1f4:	7802df0b 	zsnn \$2,\$223,\$11
 1f8:	790c14cd 	zsnn \$12,\$20,205
 1fc:	7202cb0b 	zsz \$2,\$203,\$11
 200:	730cc8cd 	zsz \$12,\$200,205
 204:	7a02cb0b 	zsnz \$2,\$203,\$11
 208:	7b0cc8cd 	zsnz \$12,\$200,205
 20c:	7402cb0b 	zsp \$2,\$203,\$11
 210:	750cc8cd 	zsp \$12,\$200,205
 214:	7c02cb0b 	zsnp \$2,\$203,\$11
 218:	7d0cc8cd 	zsnp \$12,\$200,205
 21c:	7602cb0b 	zsod \$2,\$203,\$11
 220:	770cc8cd 	zsod \$12,\$200,205
 224:	7e02cb0b 	zsev \$2,\$203,\$11
 228:	7f0cc8cd 	zsev \$12,\$200,205
 22c:	8002000b 	ldb \$2,\$0,\$11
 230:	810c14cd 	ldb \$12,\$20,205
 234:	8802000b 	ldt \$2,\$0,\$11
 238:	890c14cd 	ldt \$12,\$20,205
 23c:	8202000b 	ldbu \$2,\$0,\$11
 240:	830c14cd 	ldbu \$12,\$20,205
 244:	8a02000b 	ldtu \$2,\$0,\$11
 248:	8b0c14cd 	ldtu \$12,\$20,205
 24c:	8402000b 	ldw \$2,\$0,\$11
 250:	850c14cd 	ldw \$12,\$20,205
 254:	8c02000b 	ldo \$2,\$0,\$11
 258:	8d0c14cd 	ldo \$12,\$20,205
 25c:	8602000b 	ldwu \$2,\$0,\$11
 260:	870c14cd 	ldwu \$12,\$20,205
 264:	8e02000b 	ldou \$2,\$0,\$11
 268:	8f0c14cd 	ldou \$12,\$20,205
 26c:	9802000b 	ldvts \$2,\$0,\$11
 270:	990c14cd 	ldvts \$12,\$20,205
 274:	9202000b 	ldht \$2,\$0,\$11
 278:	930c14cd 	ldht \$12,\$20,205
 27c:	9b7014cd 	preld 112,\$20,205
 280:	9a7014e1 	preld 112,\$20,\$225
 284:	9402000b 	cswap \$2,\$0,\$11
 288:	950c14cd 	cswap \$12,\$20,205
 28c:	9d7014cd 	prego 112,\$20,205
 290:	9c7014e1 	prego 112,\$20,\$225
 294:	9602000b 	ldunc \$2,\$0,\$11
 298:	970c14cd 	ldunc \$12,\$20,205
 29c:	9e02000b 	go \$2,\$0,\$11
 2a0:	9f0c14cd 	go \$12,\$20,205
 2a4:	a0020a97 	stb \$2,\$10,\$151
 2a8:	a10c14cd 	stb \$12,\$20,205
 2ac:	a8200a97 	stt \$32,\$10,\$151
 2b0:	a90c14cd 	stt \$12,\$20,205
 2b4:	a2020a97 	stbu \$2,\$10,\$151
 2b8:	a30c14cd 	stbu \$12,\$20,205
 2bc:	aa200a97 	sttu \$32,\$10,\$151
 2c0:	ab0c14cd 	sttu \$12,\$20,205
 2c4:	a4020a97 	stw \$2,\$10,\$151
 2c8:	a50cdccd 	stw \$12,\$220,205
 2cc:	ac20aa97 	sto \$32,\$170,\$151
 2d0:	adb614f5 	sto \$182,\$20,245
 2d4:	a6020a97 	stwu \$2,\$10,\$151
 2d8:	a70cdccd 	stwu \$12,\$220,205
 2dc:	ae20aa97 	stou \$32,\$170,\$151
 2e0:	afb614f5 	stou \$182,\$20,245
 2e4:	b020aa97 	stsf \$32,\$170,\$151
 2e8:	b1b614f5 	stsf \$182,\$20,245
 2ec:	b97014cd 	syncd 112,\$20,205
 2f0:	b87014e1 	syncd 112,\$20,\$225
 2f4:	b220aa97 	stht \$32,\$170,\$151
 2f8:	b3b614f5 	stht \$182,\$20,245
 2fc:	bb7014cd 	prest 112,\$20,205
 300:	ba7014e1 	prest 112,\$20,\$225
 304:	b420aa97 	stco 32,\$170,\$151
 308:	b5b614f5 	stco 182,\$20,245
 30c:	bd7014cd 	syncid 112,\$20,205
 310:	bc0014e1 	syncid 0,\$20,\$225
 314:	b620aa97 	stunc \$32,\$170,\$151
 318:	b7b614f5 	stunc \$182,\$20,245
 31c:	be20aa97 	pushgo \$32,\$170,\$151
 320:	bfb614f5 	pushgo \$182,\$20,245
 324:	c18ec800 	set \$142,\$200
 328:	c020aa97 	or \$32,\$170,\$151
 32c:	c1b614f5 	or \$182,\$20,245
 330:	c820aa97 	and \$32,\$170,\$151
 334:	c9b614f5 	and \$182,\$20,245
 338:	c220aa97 	orn \$32,\$170,\$151
 33c:	c3b614f5 	orn \$182,\$20,245
 340:	ca20aa97 	andn \$32,\$170,\$151
 344:	cbb614f5 	andn \$182,\$20,245
 348:	c420aa97 	nor \$32,\$170,\$151
 34c:	c5b614f5 	nor \$182,\$20,245
 350:	cc20aa97 	nand \$32,\$170,\$151
 354:	cdb614f5 	nand \$182,\$20,245
 358:	c620aa97 	xor \$32,\$170,\$151
 35c:	c7b614f5 	xor \$182,\$20,245
 360:	ce20aa97 	nxor \$32,\$170,\$151
 364:	cfb614f5 	nxor \$182,\$20,245
 368:	d020aa97 	bdif \$32,\$170,\$151
 36c:	d1b614f5 	bdif \$182,\$20,245
 370:	d820aa97 	mux \$32,\$170,\$151
 374:	d9b614f5 	mux \$182,\$20,245
 378:	d220aa97 	wdif \$32,\$170,\$151
 37c:	d3b614f5 	wdif \$182,\$20,245
 380:	da20aa97 	sadd \$32,\$170,\$151
 384:	dbb600f5 	sadd \$182,\$0,245
 388:	d420aa97 	tdif \$32,\$170,\$151
 38c:	d5b614f5 	tdif \$182,\$20,245
 390:	dc20aa97 	mor \$32,\$170,\$151
 394:	ddb614f5 	mor \$182,\$20,245
 398:	d620aa97 	odif \$32,\$170,\$151
 39c:	d7b614f5 	odif \$182,\$20,245
 3a0:	de201197 	mxor \$32,\$17,\$151
 3a4:	df52b418 	mxor \$82,\$180,24
 3a8:	e004ffff 	seth \$4,0xffff
 3ac:	e05e0000 	seth \$94,0x0
 3b0:	e00400ff 	seth \$4,0xff
 3b4:	e05e04d2 	seth \$94,0x4d2
 3b8:	e15e04d2 	setmh \$94,0x4d2
 3bc:	e85e04d2 	orh \$94,0x4d2
 3c0:	e95e04d2 	ormh \$94,0x4d2
 3c4:	e25e04d2 	setml \$94,0x4d2
 3c8:	e35e04d2 	setl \$94,0x4d2
 3cc:	ea5e04d2 	orml \$94,0x4d2
 3d0:	eb5e04d2 	orl \$94,0x4d2
 3d4:	e45e04d2 	inch \$94,0x4d2
 3d8:	e55e04d2 	incmh \$94,0x4d2
 3dc:	ec5e04d2 	andnh \$94,0x4d2
 3e0:	ed5e04d2 	andnmh \$94,0x4d2
 3e4:	e65e04d2 	incml \$94,0x4d2
 3e8:	e75e04d2 	incl \$94,0x4d2
 3ec:	ee5e04d2 	andnml \$94,0x4d2
 3f0:	ef5e04d2 	andnl \$94,0x4d2
 3f4:	f1ffffff 	jmp 3f0 <Main\+0x3f0>
 3f8:	f0000001 	jmp 3fc <Main\+0x3fc>
 3fc:	f82afffe 	pop 42,65534
 400:	f90000ff 	resume 255
 404:	f9000000 	resume 0
 408:	f9000001 	resume 1
 40c:	f2190001 	pushj \$25,410 <Main\+0x410>
 410:	f319ffff 	pushj \$25,40c <Main\+0x40c>
 414:	fa040000 	save \$4,0
 418:	fb0000ea 	unsave 0,\$234
 41c:	f4190001 	geta \$25,420 <Main\+0x420>
 420:	f519ffff 	geta \$25,41c <Main\+0x41c>
 424:	fc7a1201 	sync 8000001
 428:	fd010203 	swym 1,2,3
 42c:	fd000000 	swym 0,0,0
 430:	f7040022 	put rJ,34
 434:	f6040086 	put rJ,\$134
 438:	feea0004 	get \$234,rJ
 43c:	ff000000 	trip 0,0,0
 440:	ff050607 	trip 5,6,7
