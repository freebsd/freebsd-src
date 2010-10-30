#as: -EL
#objdump: -dr
#source: allinsn.s
#name: allinsn.le

.*: +file format .*

Disassembly of section .text:

00000000 <sb>:
   0:	88 07       	sb \$7,\(\$8\)
   2:	98 05       	sb \$5,\(\$9\)
   4:	e8 07       	sb \$7,\(\$gp\)
   6:	88 0e       	sb \$gp,\(\$8\)
   8:	e8 0f       	sb \$sp,\(\$gp\)

0000000a <sh>:
   a:	89 03       	sh \$3,\(\$8\)
   c:	19 0c       	sh \$12,\(\$1\)
   e:	29 0d       	sh \$tp,\(\$2\)
  10:	89 02       	sh \$2,\(\$8\)
  12:	a9 0c       	sh \$12,\(\$10\)

00000014 <sw>:
  14:	0a 0b       	sw \$11,\(\$0\)
  16:	7a 03       	sw \$3,\(\$7\)
  18:	ea 0d       	sw \$tp,\(\$gp\)
  1a:	9a 08       	sw \$8,\(\$9\)
  1c:	8a 0e       	sw \$gp,\(\$8\)

0000001e <lb>:
  1e:	bc 0c       	lb \$12,\(\$11\)
  20:	2c 09       	lb \$9,\(\$2\)
  22:	bc 08       	lb \$8,\(\$11\)
  24:	2c 0e       	lb \$gp,\(\$2\)
  26:	cc 02       	lb \$2,\(\$12\)

00000028 <lh>:
  28:	8d 0f       	lh \$sp,\(\$8\)
  2a:	ad 03       	lh \$3,\(\$10\)
  2c:	fd 09       	lh \$9,\(\$sp\)
  2e:	fd 06       	lh \$6,\(\$sp\)
  30:	bd 0f       	lh \$sp,\(\$11\)

00000032 <lw>:
  32:	ae 0c       	lw \$12,\(\$10\)
  34:	de 09       	lw \$9,\(\$tp\)
  36:	ee 0c       	lw \$12,\(\$gp\)
  38:	be 0c       	lw \$12,\(\$11\)
  3a:	ae 0d       	lw \$tp,\(\$10\)

0000003c <lbu>:
  3c:	eb 0e       	lbu \$gp,\(\$gp\)
  3e:	8b 0c       	lbu \$12,\(\$8\)
  40:	1b 0e       	lbu \$gp,\(\$1\)
  42:	cb 08       	lbu \$8,\(\$12\)
  44:	1b 0c       	lbu \$12,\(\$1\)

00000046 <lhu>:
  46:	4f 0f       	lhu \$sp,\(\$4\)
  48:	4f 0e       	lhu \$gp,\(\$4\)
  4a:	4f 05       	lhu \$5,\(\$4\)
  4c:	df 0f       	lhu \$sp,\(\$tp\)
  4e:	ff 04       	lhu \$4,\(\$sp\)

00000050 <sw_sp>:
  50:	8a c9 03 00 	sw \$9,3\(\$8\)
  54:	5a ca 04 00 	sw \$10,4\(\$5\)
  58:	ea c0 03 00 	sw \$0,3\(\$gp\)
  5c:	8a c0 02 00 	sw \$0,2\(\$8\)
  60:	8a cf 01 00 	sw \$sp,1\(\$8\)

00000064 <lw_sp>:
  64:	5e cd 01 00 	lw \$tp,1\(\$5\)
  68:	0e cf 01 00 	lw \$sp,1\(\$0\)
  6c:	ce c0 04 00 	lw \$0,4\(\$12\)
  70:	de cb 01 00 	lw \$11,1\(\$tp\)
  74:	4e c9 03 00 	lw \$9,3\(\$4\)

00000078 <sb_tp>:
  78:	18 c5 01 00 	sb \$5,1\(\$1\)
  7c:	98 ca 01 00 	sb \$10,1\(\$9\)
  80:	38 c5 03 00 	sb \$5,3\(\$3\)
  84:	38 c5 01 00 	sb \$5,1\(\$3\)
  88:	48 ca 04 00 	sb \$10,4\(\$4\)

0000008c <sh_tp>:
  8c:	09 c3 01 00 	sh \$3,1\(\$0\)
  90:	99 cd 01 00 	sh \$tp,1\(\$9\)
  94:	a9 c9 04 00 	sh \$9,4\(\$10\)
  98:	e9 cf 03 00 	sh \$sp,3\(\$gp\)
  9c:	99 ce 04 00 	sh \$gp,4\(\$9\)

000000a0 <sw_tp>:
  a0:	da c6 02 00 	sw \$6,2\(\$tp\)
  a4:	fa c6 01 00 	sw \$6,1\(\$sp\)
  a8:	3a c2 02 00 	sw \$2,2\(\$3\)
  ac:	ca c6 02 00 	sw \$6,2\(\$12\)
  b0:	ba c3 01 00 	sw \$3,1\(\$11\)

000000b4 <lb_tp>:
  b4:	bc cd 04 00 	lb \$tp,4\(\$11\)
  b8:	8c cd 04 00 	lb \$tp,4\(\$8\)
  bc:	5c c5 04 00 	lb \$5,4\(\$5\)
  c0:	ec cf 02 00 	lb \$sp,2\(\$gp\)
  c4:	3c c3 02 00 	lb \$3,2\(\$3\)

000000c8 <lh_tp>:
  c8:	8d c7 02 00 	lh \$7,2\(\$8\)
  cc:	8d c4 03 00 	lh \$4,3\(\$8\)
  d0:	fd ce 01 00 	lh \$gp,1\(\$sp\)
  d4:	0d c9 01 00 	lh \$9,1\(\$0\)
  d8:	0d cd 02 00 	lh \$tp,2\(\$0\)

000000dc <lw_tp>:
  dc:	07 48       	lw \$8,0x4\(\$sp\)
  de:	9e cb 04 00 	lw \$11,4\(\$9\)
  e2:	2e ce 01 00 	lw \$gp,1\(\$2\)
  e6:	ee c9 02 00 	lw \$9,2\(\$gp\)
  ea:	ce c8 01 00 	lw \$8,1\(\$12\)

000000ee <lbu_tp>:
  ee:	9b cc 01 00 	lbu \$12,1\(\$9\)
  f2:	9b cb 01 00 	lbu \$11,1\(\$9\)
  f6:	8b ce 03 00 	lbu \$gp,3\(\$8\)
  fa:	fb c0 02 00 	lbu \$0,2\(\$sp\)
  fe:	bb cd 01 00 	lbu \$tp,1\(\$11\)

00000102 <lhu_tp>:
 102:	af ce 02 00 	lhu \$gp,2\(\$10\)
 106:	8f cb 01 00 	lhu \$11,1\(\$8\)
 10a:	0f c1 01 00 	lhu \$1,1\(\$0\)
 10e:	ff c7 02 00 	lhu \$7,2\(\$sp\)
 112:	83 8b       	lhu \$3,0x2\(\$tp\)

00000114 <sb16>:
 114:	b8 c7 ff ff 	sb \$7,-1\(\$11\)
 118:	e8 cd 01 00 	sb \$tp,1\(\$gp\)
 11c:	e8 c3 01 00 	sb \$3,1\(\$gp\)
 120:	68 ce 02 00 	sb \$gp,2\(\$6\)
 124:	78 ce 01 00 	sb \$gp,1\(\$7\)

00000128 <sh16>:
 128:	49 cc ff ff 	sh \$12,-1\(\$4\)
 12c:	19 cf 01 00 	sh \$sp,1\(\$1\)
 130:	c9 c2 fe ff 	sh \$2,-2\(\$12\)
 134:	b9 c9 02 00 	sh \$9,2\(\$11\)
 138:	c9 c9 fe ff 	sh \$9,-2\(\$12\)

0000013c <sw16>:
 13c:	ea cb ff ff 	sw \$11,-1\(\$gp\)
 140:	06 44       	sw \$4,0x4\(\$sp\)
 142:	3a c2 fe ff 	sw \$2,-2\(\$3\)
 146:	2a c6 ff ff 	sw \$6,-1\(\$2\)
 14a:	da c8 fe ff 	sw \$8,-2\(\$tp\)

0000014e <lb16>:
 14e:	2c ca fe ff 	lb \$10,-2\(\$2\)
 152:	bc c3 fe ff 	lb \$3,-2\(\$11\)
 156:	5c cc 01 00 	lb \$12,1\(\$5\)
 15a:	5c c5 01 00 	lb \$5,1\(\$5\)
 15e:	dc cb 02 00 	lb \$11,2\(\$tp\)

00000162 <lh16>:
 162:	bd cf ff ff 	lh \$sp,-1\(\$11\)
 166:	bd cd fe ff 	lh \$tp,-2\(\$11\)
 16a:	ad c2 01 00 	lh \$2,1\(\$10\)
 16e:	7d c8 ff ff 	lh \$8,-1\(\$7\)
 172:	bd ce ff ff 	lh \$gp,-1\(\$11\)

00000176 <lw16>:
 176:	5e c0 ff ff 	lw \$0,-1\(\$5\)
 17a:	7e cc fe ff 	lw \$12,-2\(\$7\)
 17e:	3e c1 fe ff 	lw \$1,-2\(\$3\)
 182:	7e c1 02 00 	lw \$1,2\(\$7\)
 186:	8e c4 01 00 	lw \$4,1\(\$8\)

0000018a <lbu16>:
 18a:	4b cc ff ff 	lbu \$12,-1\(\$4\)
 18e:	bb ce 01 00 	lbu \$gp,1\(\$11\)
 192:	db c1 ff ff 	lbu \$1,-1\(\$tp\)
 196:	db c9 ff ff 	lbu \$9,-1\(\$tp\)
 19a:	fb c8 01 00 	lbu \$8,1\(\$sp\)

0000019e <lhu16>:
 19e:	ff cd ff ff 	lhu \$tp,-1\(\$sp\)
 1a2:	8f ce 02 00 	lhu \$gp,2\(\$8\)
 1a6:	cf cf ff ff 	lhu \$sp,-1\(\$12\)
 1aa:	0f c3 ff ff 	lhu \$3,-1\(\$0\)
 1ae:	cf c3 fe ff 	lhu \$3,-2\(\$12\)

000001b2 <sw24>:
 1b2:	06 eb 00 00 	sw \$11,\(0x4\)
 1b6:	06 ef 00 00 	sw \$sp,\(0x4\)
 1ba:	0a e7 00 00 	sw \$7,\(0x8\)
 1be:	12 ea 00 00 	sw \$10,\(0x10\)
 1c2:	a2 e8 00 00 	sw \$8,\(0xa0\)

000001c6 <lw24>:
 1c6:	07 e4 00 00 	lw \$4,\(0x4\)
 1ca:	07 ef 00 00 	lw \$sp,\(0x4\)
 1ce:	13 e4 00 00 	lw \$4,\(0x10\)
 1d2:	03 e8 00 00 	lw \$8,\(0x0\)
 1d6:	0b ed 00 00 	lw \$tp,\(0x8\)

000001da <extb>:
 1da:	0d 1d       	extb \$tp
 1dc:	0d 1d       	extb \$tp
 1de:	0d 16       	extb \$6
 1e0:	0d 1e       	extb \$gp
 1e2:	0d 1a       	extb \$10

000001e4 <exth>:
 1e4:	2d 1f       	exth \$sp
 1e6:	2d 12       	exth \$2
 1e8:	2d 15       	exth \$5
 1ea:	2d 1a       	exth \$10
 1ec:	2d 14       	exth \$4

000001ee <extub>:
 1ee:	8d 12       	extub \$2
 1f0:	8d 1d       	extub \$tp
 1f2:	8d 13       	extub \$3
 1f4:	8d 19       	extub \$9
 1f6:	8d 1e       	extub \$gp

000001f8 <extuh>:
 1f8:	ad 18       	extuh \$8
 1fa:	ad 18       	extuh \$8
 1fc:	ad 14       	extuh \$4
 1fe:	ad 10       	extuh \$0
 200:	ad 10       	extuh \$0

00000202 <ssarb>:
 202:	8c 12       	ssarb 2\(\$8\)
 204:	dc 12       	ssarb 2\(\$tp\)
 206:	dc 11       	ssarb 1\(\$tp\)
 208:	5c 12       	ssarb 2\(\$5\)
 20a:	9c 10       	ssarb 0\(\$9\)

0000020c <mov>:
 20c:	30 02       	mov \$2,\$3
 20e:	b0 03       	mov \$3,\$11
 210:	a0 0f       	mov \$sp,\$10
 212:	00 0f       	mov \$sp,\$0
 214:	d0 03       	mov \$3,\$tp

00000216 <movi8>:
 216:	ff 5b       	mov \$11,-1
 218:	02 56       	mov \$6,2
 21a:	ff 5f       	mov \$sp,-1
 21c:	01 5f       	mov \$sp,1
 21e:	ff 5e       	mov \$gp,-1

00000220 <movi16>:
 220:	00 5f       	mov \$sp,0
 222:	02 50       	mov \$0,2
 224:	ff 58       	mov \$8,-1
 226:	01 5c       	mov \$12,1
 228:	ff 57       	mov \$7,-1

0000022a <movu24>:
 22a:	01 d2 00 00 	movu \$2,0x1
 22e:	11 ca 04 00 	movu \$10,0x4
 232:	11 c9 00 00 	movu \$9,0x0
 236:	03 d4 00 00 	movu \$4,0x3
 23a:	11 ce 01 00 	movu \$gp,0x1

0000023e <movu16>:
 23e:	11 cf 01 00 	movu \$sp,0x1
 242:	03 d6 00 00 	movu \$6,0x3
 246:	03 d0 00 00 	movu \$0,0x3
 24a:	11 ce 03 00 	movu \$gp,0x3
 24e:	11 ca 02 00 	movu \$10,0x2

00000252 <movh>:
 252:	21 c8 02 00 	movh \$8,0x2
 256:	21 cd 01 00 	movh \$tp,0x1
 25a:	21 ce 02 00 	movh \$gp,0x2
 25e:	21 cc 00 00 	movh \$12,0x0
 262:	21 cb 02 00 	movh \$11,0x2

00000266 <add3>:
 266:	36 9b       	add3 \$6,\$11,\$3
 268:	5e 9d       	add3 \$gp,\$tp,\$5
 26a:	73 9b       	add3 \$3,\$11,\$7
 26c:	dd 9e       	add3 \$tp,\$gp,\$tp
 26e:	80 9e       	add3 \$0,\$gp,\$8

00000270 <add>:
 270:	08 6c       	add \$12,2
 272:	fc 6c       	add \$12,-1
 274:	04 64       	add \$4,1
 276:	04 66       	add \$6,1
 278:	08 66       	add \$6,2

0000027a <add3i>:
 27a:	04 4b       	add3 \$11,\$sp,0x4
 27c:	f0 c4 01 00 	add3 \$4,\$sp,1
 280:	00 40       	add3 \$0,\$sp,0x0
 282:	f0 cd 03 00 	add3 \$tp,\$sp,3
 286:	00 4b       	add3 \$11,\$sp,0x0

00000288 <advck3>:
 288:	a7 0e       	advck3 \$0,\$gp,\$10
 28a:	07 0d       	advck3 \$0,\$tp,\$0
 28c:	d7 0e       	advck3 \$0,\$gp,\$tp
 28e:	87 07       	advck3 \$0,\$7,\$8
 290:	27 01       	advck3 \$0,\$1,\$2

00000292 <sub>:
 292:	e4 08       	sub \$8,\$gp
 294:	94 01       	sub \$1,\$9
 296:	74 0d       	sub \$tp,\$7
 298:	34 0f       	sub \$sp,\$3
 29a:	74 02       	sub \$2,\$7

0000029c <sbvck3>:
 29c:	e5 03       	sbvck3 \$0,\$3,\$gp
 29e:	75 03       	sbvck3 \$0,\$3,\$7
 2a0:	a5 0a       	sbvck3 \$0,\$10,\$10
 2a2:	d5 04       	sbvck3 \$0,\$4,\$tp
 2a4:	f5 0a       	sbvck3 \$0,\$10,\$sp

000002a6 <neg>:
 2a6:	71 0e       	neg \$gp,\$7
 2a8:	71 01       	neg \$1,\$7
 2aa:	b1 02       	neg \$2,\$11
 2ac:	81 0d       	neg \$tp,\$8
 2ae:	d1 0e       	neg \$gp,\$tp

000002b0 <slt3>:
 2b0:	82 0e       	slt3 \$0,\$gp,\$8
 2b2:	d2 04       	slt3 \$0,\$4,\$tp
 2b4:	e2 0a       	slt3 \$0,\$10,\$gp
 2b6:	52 0e       	slt3 \$0,\$gp,\$5
 2b8:	c2 03       	slt3 \$0,\$3,\$12

000002ba <sltu3>:
 2ba:	83 02       	sltu3 \$0,\$2,\$8
 2bc:	b3 0e       	sltu3 \$0,\$gp,\$11
 2be:	d3 02       	sltu3 \$0,\$2,\$tp
 2c0:	83 09       	sltu3 \$0,\$9,\$8
 2c2:	93 06       	sltu3 \$0,\$6,\$9

000002c4 <slt3i>:
 2c4:	11 66       	slt3 \$0,\$6,0x2
 2c6:	09 6b       	slt3 \$0,\$11,0x1
 2c8:	01 6f       	slt3 \$0,\$sp,0x0
 2ca:	01 63       	slt3 \$0,\$3,0x0
 2cc:	01 6d       	slt3 \$0,\$tp,0x0

000002ce <sltu3i>:
 2ce:	25 6e       	sltu3 \$0,\$gp,0x4
 2d0:	1d 6d       	sltu3 \$0,\$tp,0x3
 2d2:	0d 63       	sltu3 \$0,\$3,0x1
 2d4:	05 6c       	sltu3 \$0,\$12,0x0
 2d6:	1d 61       	sltu3 \$0,\$1,0x3

000002d8 <sl1ad3>:
 2d8:	e6 28       	sl1ad3 \$0,\$8,\$gp
 2da:	26 24       	sl1ad3 \$0,\$4,\$2
 2dc:	c6 2f       	sl1ad3 \$0,\$sp,\$12
 2de:	16 29       	sl1ad3 \$0,\$9,\$1
 2e0:	26 28       	sl1ad3 \$0,\$8,\$2

000002e2 <sl2ad3>:
 2e2:	d7 28       	sl2ad3 \$0,\$8,\$tp
 2e4:	37 22       	sl2ad3 \$0,\$2,\$3
 2e6:	97 28       	sl2ad3 \$0,\$8,\$9
 2e8:	c7 27       	sl2ad3 \$0,\$7,\$12
 2ea:	c7 24       	sl2ad3 \$0,\$4,\$12

000002ec <add3x>:
 2ec:	b0 cd 01 00 	add3 \$tp,\$11,1
 2f0:	40 cd ff ff 	add3 \$tp,\$4,-1
 2f4:	d0 c2 01 00 	add3 \$2,\$tp,1
 2f8:	e0 c3 01 00 	add3 \$3,\$gp,1
 2fc:	f0 ca 02 00 	add3 \$10,\$sp,2

00000300 <slt3x>:
 300:	12 c8 ff ff 	slt3 \$8,\$1,-1
 304:	32 c0 fe ff 	slt3 \$0,\$3,-2
 308:	f2 c9 ff ff 	slt3 \$9,\$sp,-1
 30c:	82 c3 02 00 	slt3 \$3,\$8,2
 310:	e2 cd 00 00 	slt3 \$tp,\$gp,0

00000314 <sltu3x>:
 314:	b3 cf 02 00 	sltu3 \$sp,\$11,0x2
 318:	03 c6 01 00 	sltu3 \$6,\$0,0x1
 31c:	b3 c9 03 00 	sltu3 \$9,\$11,0x3
 320:	05 64       	sltu3 \$0,\$4,0x0
 322:	e3 cd 04 00 	sltu3 \$tp,\$gp,0x4

00000326 <or>:
 326:	e0 1f       	or \$sp,\$gp
 328:	30 18       	or \$8,\$3
 32a:	f0 10       	or \$0,\$sp
 32c:	00 1d       	or \$tp,\$0
 32e:	60 18       	or \$8,\$6

00000330 <and>:
 330:	f1 1f       	and \$sp,\$sp
 332:	e1 16       	and \$6,\$gp
 334:	21 14       	and \$4,\$2
 336:	81 15       	and \$5,\$8
 338:	e1 17       	and \$7,\$gp

0000033a <xor>:
 33a:	c2 11       	xor \$1,\$12
 33c:	d2 1c       	xor \$12,\$tp
 33e:	82 1a       	xor \$10,\$8
 340:	b2 1f       	xor \$sp,\$11
 342:	82 1c       	xor \$12,\$8

00000344 <nor>:
 344:	53 19       	nor \$9,\$5
 346:	23 18       	nor \$8,\$2
 348:	93 1f       	nor \$sp,\$9
 34a:	f3 15       	nor \$5,\$sp
 34c:	e3 1f       	nor \$sp,\$gp

0000034e <or3>:
 34e:	f4 cd 02 00 	or3 \$tp,\$sp,0x2
 352:	d4 cf 03 00 	or3 \$sp,\$tp,0x3
 356:	a4 c0 04 00 	or3 \$0,\$10,0x4
 35a:	f4 c9 03 00 	or3 \$9,\$sp,0x3
 35e:	f4 c9 00 00 	or3 \$9,\$sp,0x0

00000362 <and3>:
 362:	85 c5 01 00 	and3 \$5,\$8,0x1
 366:	e5 cb 03 00 	and3 \$11,\$gp,0x3
 36a:	05 c6 00 00 	and3 \$6,\$0,0x0
 36e:	f5 cf 00 00 	and3 \$sp,\$sp,0x0
 372:	a5 c1 03 00 	and3 \$1,\$10,0x3

00000376 <xor3>:
 376:	06 c0 02 00 	xor3 \$0,\$0,0x2
 37a:	66 cf 00 00 	xor3 \$sp,\$6,0x0
 37e:	56 cd 00 00 	xor3 \$tp,\$5,0x0
 382:	76 cf 00 00 	xor3 \$sp,\$7,0x0
 386:	f6 cf 02 00 	xor3 \$sp,\$sp,0x2

0000038a <sra>:
 38a:	1d 24       	sra \$4,\$1
 38c:	fd 28       	sra \$8,\$sp
 38e:	1d 21       	sra \$1,\$1
 390:	5d 20       	sra \$0,\$5
 392:	1d 29       	sra \$9,\$1

00000394 <srl>:
 394:	bc 22       	srl \$2,\$11
 396:	7c 2f       	srl \$sp,\$7
 398:	7c 21       	srl \$1,\$7
 39a:	dc 23       	srl \$3,\$tp
 39c:	1c 2e       	srl \$gp,\$1

0000039e <sll>:
 39e:	0e 2b       	sll \$11,\$0
 3a0:	8e 2d       	sll \$tp,\$8
 3a2:	9e 28       	sll \$8,\$9
 3a4:	fe 2d       	sll \$tp,\$sp
 3a6:	fe 2f       	sll \$sp,\$sp

000003a8 <srai>:
 3a8:	13 61       	sra \$1,0x2
 3aa:	1b 6f       	sra \$sp,0x3
 3ac:	1b 6f       	sra \$sp,0x3
 3ae:	23 66       	sra \$6,0x4
 3b0:	1b 6f       	sra \$sp,0x3

000003b2 <srli>:
 3b2:	02 6a       	srl \$10,0x0
 3b4:	1a 69       	srl \$9,0x3
 3b6:	22 66       	srl \$6,0x4
 3b8:	12 6a       	srl \$10,0x2
 3ba:	1a 68       	srl \$8,0x3

000003bc <slli>:
 3bc:	06 60       	sll \$0,0x0
 3be:	06 64       	sll \$4,0x0
 3c0:	16 6d       	sll \$tp,0x2
 3c2:	16 6b       	sll \$11,0x2
 3c4:	06 66       	sll \$6,0x0

000003c6 <sll3>:
 3c6:	27 6d       	sll3 \$0,\$tp,0x4
 3c8:	07 6e       	sll3 \$0,\$gp,0x0
 3ca:	17 68       	sll3 \$0,\$8,0x2
 3cc:	17 63       	sll3 \$0,\$3,0x2
 3ce:	07 68       	sll3 \$0,\$8,0x0

000003d0 <fsft>:
 3d0:	af 2e       	fsft \$gp,\$10
 3d2:	9f 2e       	fsft \$gp,\$9
 3d4:	df 2f       	fsft \$sp,\$tp
 3d6:	3f 2b       	fsft \$11,\$3
 3d8:	3f 25       	fsft \$5,\$3

000003da <bra>:
 3da:	02 b0       	bra 3dc <bra\+0x2>
 3dc:	fe bf       	bra 3da <bra>
 3de:	02 b0       	bra 3e0 <bra\+0x6>
 3e0:	00 b0       	bra 3e0 <bra\+0x6>
 3e2:	02 b0       	bra 3e4 <beqz>

000003e4 <beqz>:
 3e4:	fe a1       	beqz \$1,3e2 <bra\+0x8>
 3e6:	02 af       	beqz \$sp,3e8 <beqz\+0x4>
 3e8:	04 a4       	beqz \$4,3ec <beqz\+0x8>
 3ea:	00 a4       	beqz \$4,3ea <beqz\+0x6>
 3ec:	fe a9       	beqz \$9,3ea <beqz\+0x6>

000003ee <bnez>:
 3ee:	03 a8       	bnez \$8,3f0 <bnez\+0x2>
 3f0:	03 ad       	bnez \$tp,3f2 <bnez\+0x4>
 3f2:	01 ae       	bnez \$gp,3f2 <bnez\+0x4>
 3f4:	03 a6       	bnez \$6,3f6 <bnez\+0x8>
 3f6:	fd a8       	bnez \$8,3f2 <bnez\+0x4>

000003f8 <beqi>:
 3f8:	30 ed 00 00 	beqi \$tp,0x3,3f8 <beqi>
 3fc:	40 e0 ff ff 	beqi \$0,0x4,3fa <beqi\+0x2>
 400:	40 ef ff ff 	beqi \$sp,0x4,3fe <beqi\+0x6>
 404:	20 ed 00 00 	beqi \$tp,0x2,404 <beqi\+0xc>
 408:	20 e4 fc ff 	beqi \$4,0x2,400 <beqi\+0x8>

0000040c <bnei>:
 40c:	14 e8 00 00 	bnei \$8,0x1,40c <bnei>
 410:	14 e5 01 00 	bnei \$5,0x1,412 <bnei\+0x6>
 414:	04 e5 04 00 	bnei \$5,0x0,41c <bnei\+0x10>
 418:	44 e9 ff ff 	bnei \$9,0x4,416 <bnei\+0xa>
 41c:	44 e0 fc ff 	bnei \$0,0x4,414 <bnei\+0x8>

00000420 <blti>:
 420:	3c e7 00 00 	blti \$7,0x3,420 <blti>
 424:	1c e1 00 00 	blti \$1,0x1,424 <blti\+0x4>
 428:	2c e8 01 00 	blti \$8,0x2,42a <blti\+0xa>
 42c:	2c eb 01 00 	blti \$11,0x2,42e <blti\+0xe>
 430:	3c ef ff ff 	blti \$sp,0x3,42e <blti\+0xe>

00000434 <bgei>:
 434:	38 e4 fc ff 	bgei \$4,0x3,42c <blti\+0xc>
 438:	08 e7 01 00 	bgei \$7,0x0,43a <bgei\+0x6>
 43c:	18 ed 00 00 	bgei \$tp,0x1,43c <bgei\+0x8>
 440:	28 e5 ff ff 	bgei \$5,0x2,43e <bgei\+0xa>
 444:	48 ec fc ff 	bgei \$12,0x4,43c <bgei\+0x8>

00000448 <beq>:
 448:	21 e7 ff ff 	beq \$7,\$2,446 <bgei\+0x12>
 44c:	31 e1 fc ff 	beq \$1,\$3,444 <bgei\+0x10>
 450:	01 e2 01 00 	beq \$2,\$0,452 <beq\+0xa>
 454:	81 ef 01 00 	beq \$sp,\$8,456 <beq\+0xe>
 458:	01 e3 00 00 	beq \$3,\$0,458 <beq\+0x10>

0000045c <bne>:
 45c:	35 e6 00 00 	bne \$6,\$3,45c <bne>
 460:	35 ef fc ff 	bne \$sp,\$3,458 <beq\+0x10>
 464:	05 e8 01 00 	bne \$8,\$0,466 <bne\+0xa>
 468:	f5 ee 04 00 	bne \$gp,\$sp,470 <bsr12>
 46c:	45 ef 01 00 	bne \$sp,\$4,46e <bne\+0x12>

00000470 <bsr12>:
 470:	03 b0       	bsr 472 <bsr12\+0x2>
 472:	f9 bf       	bsr 46a <bne\+0xe>
 474:	f1 bf       	bsr 464 <bne\+0x8>
 476:	ff bf       	bsr 474 <bsr12\+0x4>
 478:	f9 bf       	bsr 470 <bsr12>

0000047a <bsr24>:
 47a:	05 b0       	bsr 47e <bsr24\+0x4>
 47c:	ff bf       	bsr 47a <bsr24>
 47e:	fd bf       	bsr 47a <bsr24>
 480:	01 b0       	bsr 480 <bsr24\+0x6>
 482:	03 b0       	bsr 484 <jmp>

00000484 <jmp>:
 484:	2e 10       	jmp \$2
 486:	de 10       	jmp \$tp
 488:	5e 10       	jmp \$5
 48a:	fe 10       	jmp \$sp
 48c:	8e 10       	jmp \$8

0000048e <jmp24>:
 48e:	28 d8 00 00 	jmp 4 <sb\+0x4>
 492:	18 d8 00 00 	jmp 2 <sb\+0x2>
 496:	08 d8 00 00 	jmp 0 <sb>
 49a:	18 d8 00 00 	jmp 2 <sb\+0x2>
 49e:	28 d8 00 00 	jmp 4 <sb\+0x4>

000004a2 <jsr>:
 4a2:	ff 10       	jsr \$sp
 4a4:	df 10       	jsr \$tp
 4a6:	df 10       	jsr \$tp
 4a8:	6f 10       	jsr \$6
 4aa:	6f 10       	jsr \$6

000004ac <ret>:
 4ac:	02 70       	ret

000004ae <repeat>:
 4ae:	09 e4 01 00 	repeat \$4,4b0 <repeat\+0x2>
 4b2:	09 e8 02 00 	repeat \$8,4b6 <repeat\+0x8>
 4b6:	09 e0 04 00 	repeat \$0,4be <repeat\+0x10>
 4ba:	09 e6 01 00 	repeat \$6,4bc <repeat\+0xe>
 4be:	09 e4 01 00 	repeat \$4,4c0 <repeat\+0x12>

000004c2 <erepeat>:
 4c2:	19 e0 01 00 	erepeat 4c4 <erepeat\+0x2>
 4c6:	19 e0 00 00 	erepeat 4c6 <erepeat\+0x4>
 4ca:	19 e0 01 00 	erepeat 4cc <erepeat\+0xa>
 4ce:	19 e0 ff ff 	erepeat 4cc <erepeat\+0xa>
 4d2:	19 e0 00 00 	erepeat 4d2 <erepeat\+0x10>

000004d6 <stc>:
 4d6:	e8 7d       	stc \$tp,\$mb1
 4d8:	c9 7d       	stc \$tp,\$ccfg
 4da:	89 7b       	stc \$11,\$dbg
 4dc:	c9 7a       	stc \$10,\$ccfg
 4de:	39 79       	stc \$9,\$epc

000004e0 <ldc>:
 4e0:	8a 7d       	ldc \$tp,\$lo
 4e2:	7b 78       	ldc \$8,\$npc
 4e4:	ca 79       	ldc \$9,\$mb0
 4e6:	2a 7f       	ldc \$sp,\$sar
 4e8:	cb 79       	ldc \$9,\$ccfg

000004ea <di>:
 4ea:	00 70       	di

000004ec <ei>:
 4ec:	10 70       	ei

000004ee <reti>:
 4ee:	12 70       	reti

000004f0 <halt>:
 4f0:	22 70       	halt

000004f2 <swi>:
 4f2:	26 70       	swi 0x2
 4f4:	06 70       	swi 0x0
 4f6:	26 70       	swi 0x2
 4f8:	36 70       	swi 0x3
 4fa:	16 70       	swi 0x1

000004fc <break>:
 4fc:	32 70       	break

000004fe <syncm>:
 4fe:	11 70       	syncm

00000500 <stcb>:
 500:	04 f5 04 00 	stcb \$5,0x4
 504:	04 f5 01 00 	stcb \$5,0x1
 508:	04 fe 00 00 	stcb \$gp,0x0
 50c:	04 ff 04 00 	stcb \$sp,0x4
 510:	04 fb 02 00 	stcb \$11,0x2

00000514 <ldcb>:
 514:	14 f2 03 00 	ldcb \$2,0x3
 518:	14 f2 04 00 	ldcb \$2,0x4
 51c:	14 f9 01 00 	ldcb \$9,0x1
 520:	14 fa 04 00 	ldcb \$10,0x4
 524:	14 f1 04 00 	ldcb \$1,0x4

00000528 <bsetm>:
 528:	a0 20       	bsetm \(\$10\),0x0
 52a:	f0 20       	bsetm \(\$sp\),0x0
 52c:	10 22       	bsetm \(\$1\),0x2
 52e:	f0 24       	bsetm \(\$sp\),0x4
 530:	80 24       	bsetm \(\$8\),0x4

00000532 <bclrm>:
 532:	51 20       	bclrm \(\$5\),0x0
 534:	51 22       	bclrm \(\$5\),0x2
 536:	81 20       	bclrm \(\$8\),0x0
 538:	91 22       	bclrm \(\$9\),0x2
 53a:	51 23       	bclrm \(\$5\),0x3

0000053c <bnotm>:
 53c:	e2 24       	bnotm \(\$gp\),0x4
 53e:	b2 24       	bnotm \(\$11\),0x4
 540:	a2 20       	bnotm \(\$10\),0x0
 542:	d2 24       	bnotm \(\$tp\),0x4
 544:	82 20       	bnotm \(\$8\),0x0

00000546 <btstm>:
 546:	e3 20       	btstm \$0,\(\$gp\),0x0
 548:	e3 21       	btstm \$0,\(\$gp\),0x1
 54a:	b3 20       	btstm \$0,\(\$11\),0x0
 54c:	e3 23       	btstm \$0,\(\$gp\),0x3
 54e:	83 22       	btstm \$0,\(\$8\),0x2

00000550 <tas>:
 550:	d4 27       	tas \$7,\(\$tp\)
 552:	c4 27       	tas \$7,\(\$12\)
 554:	84 23       	tas \$3,\(\$8\)
 556:	54 22       	tas \$2,\(\$5\)
 558:	a4 26       	tas \$6,\(\$10\)

0000055a <cache>:
 55a:	d4 71       	cache 0x1,\(\$tp\)
 55c:	c4 73       	cache 0x3,\(\$12\)
 55e:	94 73       	cache 0x3,\(\$9\)
 560:	24 74       	cache 0x4,\(\$2\)
 562:	74 74       	cache 0x4,\(\$7\)

00000564 <mul>:
 564:	e4 18       	mul \$8,\$gp
 566:	94 12       	mul \$2,\$9
 568:	f4 1e       	mul \$gp,\$sp
 56a:	74 19       	mul \$9,\$7
 56c:	b4 17       	mul \$7,\$11

0000056e <mulu>:
 56e:	55 12       	mulu \$2,\$5
 570:	e5 16       	mulu \$6,\$gp
 572:	f5 1e       	mulu \$gp,\$sp
 574:	e5 1b       	mulu \$11,\$gp
 576:	95 13       	mulu \$3,\$9

00000578 <mulr>:
 578:	66 1c       	mulr \$12,\$6
 57a:	86 1d       	mulr \$tp,\$8
 57c:	a6 17       	mulr \$7,\$10
 57e:	16 1e       	mulr \$gp,\$1
 580:	f6 10       	mulr \$0,\$sp

00000582 <mulru>:
 582:	27 14       	mulru \$4,\$2
 584:	17 1e       	mulru \$gp,\$1
 586:	47 1f       	mulru \$sp,\$4
 588:	67 1a       	mulru \$10,\$6
 58a:	e7 10       	mulru \$0,\$gp

0000058c <madd>:
 58c:	b1 f4 04 30 	madd \$4,\$11
 590:	e1 ff 04 30 	madd \$sp,\$gp
 594:	f1 fe 04 30 	madd \$gp,\$sp
 598:	d1 f4 04 30 	madd \$4,\$tp
 59c:	e1 f1 04 30 	madd \$1,\$gp

000005a0 <maddu>:
 5a0:	11 f0 05 30 	maddu \$0,\$1
 5a4:	61 f7 05 30 	maddu \$7,\$6
 5a8:	51 f9 05 30 	maddu \$9,\$5
 5ac:	f1 fe 05 30 	maddu \$gp,\$sp
 5b0:	d1 f7 05 30 	maddu \$7,\$tp

000005b4 <maddr>:
 5b4:	81 f6 06 30 	maddr \$6,\$8
 5b8:	e1 f9 06 30 	maddr \$9,\$gp
 5bc:	e1 f8 06 30 	maddr \$8,\$gp
 5c0:	21 f3 06 30 	maddr \$3,\$2
 5c4:	b1 f1 06 30 	maddr \$1,\$11

000005c8 <maddru>:
 5c8:	31 fa 07 30 	maddru \$10,\$3
 5cc:	c1 ff 07 30 	maddru \$sp,\$12
 5d0:	81 f8 07 30 	maddru \$8,\$8
 5d4:	31 fe 07 30 	maddru \$gp,\$3
 5d8:	f1 f8 07 30 	maddru \$8,\$sp

000005dc <div>:
 5dc:	38 19       	div \$9,\$3
 5de:	e8 14       	div \$4,\$gp
 5e0:	c8 12       	div \$2,\$12
 5e2:	d8 18       	div \$8,\$tp
 5e4:	68 1d       	div \$tp,\$6

000005e6 <divu>:
 5e6:	59 19       	divu \$9,\$5
 5e8:	d9 18       	divu \$8,\$tp
 5ea:	e9 10       	divu \$0,\$gp
 5ec:	59 19       	divu \$9,\$5
 5ee:	59 10       	divu \$0,\$5

000005f0 <dret>:
 5f0:	13 70       	dret

000005f2 <dbreak>:
 5f2:	33 70       	dbreak

000005f4 <ldz>:
 5f4:	41 fe 00 00 	ldz \$gp,\$4
 5f8:	b1 fa 00 00 	ldz \$10,\$11
 5fc:	91 f9 00 00 	ldz \$9,\$9
 600:	d1 ff 00 00 	ldz \$sp,\$tp
 604:	31 fe 00 00 	ldz \$gp,\$3

00000608 <abs>:
 608:	91 ff 03 00 	abs \$sp,\$9
 60c:	41 f5 03 00 	abs \$5,\$4
 610:	d1 fd 03 00 	abs \$tp,\$tp
 614:	31 f0 03 00 	abs \$0,\$3
 618:	e1 f3 03 00 	abs \$3,\$gp

0000061c <ave>:
 61c:	a1 fb 02 00 	ave \$11,\$10
 620:	a1 f8 02 00 	ave \$8,\$10
 624:	21 fe 02 00 	ave \$gp,\$2
 628:	c1 fa 02 00 	ave \$10,\$12
 62c:	81 ff 02 00 	ave \$sp,\$8

00000630 <min>:
 630:	31 f8 04 00 	min \$8,\$3
 634:	01 f7 04 00 	min \$7,\$0
 638:	21 f2 04 00 	min \$2,\$2
 63c:	61 f5 04 00 	min \$5,\$6
 640:	51 fb 04 00 	min \$11,\$5

00000644 <max>:
 644:	f1 fb 05 00 	max \$11,\$sp
 648:	01 fe 05 00 	max \$gp,\$0
 64c:	f1 fc 05 00 	max \$12,\$sp
 650:	21 fe 05 00 	max \$gp,\$2
 654:	f1 fe 05 00 	max \$gp,\$sp

00000658 <minu>:
 658:	81 fb 06 00 	minu \$11,\$8
 65c:	51 f7 06 00 	minu \$7,\$5
 660:	e1 f8 06 00 	minu \$8,\$gp
 664:	41 fb 06 00 	minu \$11,\$4
 668:	f1 f2 06 00 	minu \$2,\$sp

0000066c <maxu>:
 66c:	31 f3 07 00 	maxu \$3,\$3
 670:	01 fd 07 00 	maxu \$tp,\$0
 674:	81 f4 07 00 	maxu \$4,\$8
 678:	21 fe 07 00 	maxu \$gp,\$2
 67c:	81 fc 07 00 	maxu \$12,\$8

00000680 <clip>:
 680:	01 fa 08 10 	clip \$10,0x1
 684:	01 ff 20 10 	clip \$sp,0x4
 688:	01 f4 18 10 	clip \$4,0x3
 68c:	01 ff 18 10 	clip \$sp,0x3
 690:	01 f1 00 10 	clip \$1,0x0

00000694 <clipu>:
 694:	01 fa 21 10 	clipu \$10,0x4
 698:	01 fd 09 10 	clipu \$tp,0x1
 69c:	01 f5 21 10 	clipu \$5,0x4
 6a0:	01 fe 01 10 	clipu \$gp,0x0
 6a4:	01 f5 09 10 	clipu \$5,0x1

000006a8 <sadd>:
 6a8:	01 f5 08 00 	sadd \$5,\$0
 6ac:	31 ff 08 00 	sadd \$sp,\$3
 6b0:	a1 f0 08 00 	sadd \$0,\$10
 6b4:	c1 ff 08 00 	sadd \$sp,\$12
 6b8:	21 f4 08 00 	sadd \$4,\$2

000006bc <ssub>:
 6bc:	a1 f1 0a 00 	ssub \$1,\$10
 6c0:	71 f4 0a 00 	ssub \$4,\$7
 6c4:	31 f8 0a 00 	ssub \$8,\$3
 6c8:	e1 f7 0a 00 	ssub \$7,\$gp
 6cc:	41 fd 0a 00 	ssub \$tp,\$4

000006d0 <saddu>:
 6d0:	e1 f9 09 00 	saddu \$9,\$gp
 6d4:	a1 f0 09 00 	saddu \$0,\$10
 6d8:	c1 f7 09 00 	saddu \$7,\$12
 6dc:	f1 f5 09 00 	saddu \$5,\$sp
 6e0:	31 fd 09 00 	saddu \$tp,\$3

000006e4 <ssubu>:
 6e4:	e1 ff 0b 00 	ssubu \$sp,\$gp
 6e8:	f1 f0 0b 00 	ssubu \$0,\$sp
 6ec:	a1 f3 0b 00 	ssubu \$3,\$10
 6f0:	d1 ff 0b 00 	ssubu \$sp,\$tp
 6f4:	91 f2 0b 00 	ssubu \$2,\$9

000006f8 <swcp>:
 6f8:	d8 33       	swcp \$c3,\(\$tp\)
 6fa:	d8 3f       	swcp \$c15,\(\$tp\)
 6fc:	08 3d       	swcp \$c13,\(\$0\)
 6fe:	c8 3c       	swcp \$c12,\(\$12\)
 700:	e8 39       	swcp \$c9,\(\$gp\)

00000702 <lwcp>:
 702:	39 37       	lwcp \$c7,\(\$3\)
 704:	39 36       	lwcp \$c6,\(\$3\)
 706:	29 30       	lwcp \$c0,\(\$2\)
 708:	89 38       	lwcp \$c8,\(\$8\)
 70a:	d9 3b       	lwcp \$c11,\(\$tp\)

0000070c <smcp>:
 70c:	9a 3e       	smcp \$c14,\(\$9\)
 70e:	8a 32       	smcp \$c2,\(\$8\)
 710:	fa 3e       	smcp \$c14,\(\$sp\)
 712:	8a 3a       	smcp \$c10,\(\$8\)
 714:	8a 32       	smcp \$c2,\(\$8\)

00000716 <lmcp>:
 716:	1b 3b       	lmcp \$c11,\(\$1\)
 718:	8b 38       	lmcp \$c8,\(\$8\)
 71a:	db 3b       	lmcp \$c11,\(\$tp\)
 71c:	0b 38       	lmcp \$c8,\(\$0\)
 71e:	eb 38       	lmcp \$c8,\(\$gp\)

00000720 <swcpi>:
 720:	00 37       	swcpi \$c7,\(\$0\+\)
 722:	e0 36       	swcpi \$c6,\(\$gp\+\)
 724:	80 3c       	swcpi \$c12,\(\$8\+\)
 726:	f0 3e       	swcpi \$c14,\(\$sp\+\)
 728:	00 36       	swcpi \$c6,\(\$0\+\)

0000072a <lwcpi>:
 72a:	21 38       	lwcpi \$c8,\(\$2\+\)
 72c:	01 39       	lwcpi \$c9,\(\$0\+\)
 72e:	e1 33       	lwcpi \$c3,\(\$gp\+\)
 730:	51 3d       	lwcpi \$c13,\(\$5\+\)
 732:	e1 3b       	lwcpi \$c11,\(\$gp\+\)

00000734 <smcpi>:
 734:	22 38       	smcpi \$c8,\(\$2\+\)
 736:	92 3b       	smcpi \$c11,\(\$9\+\)
 738:	32 34       	smcpi \$c4,\(\$3\+\)
 73a:	22 3e       	smcpi \$c14,\(\$2\+\)
 73c:	32 39       	smcpi \$c9,\(\$3\+\)

0000073e <lmcpi>:
 73e:	e3 36       	lmcpi \$c6,\(\$gp\+\)
 740:	53 39       	lmcpi \$c9,\(\$5\+\)
 742:	63 3a       	lmcpi \$c10,\(\$6\+\)
 744:	63 31       	lmcpi \$c1,\(\$6\+\)
 746:	83 32       	lmcpi \$c2,\(\$8\+\)

00000748 <swcp16>:
 748:	2c f0 ff ff 	swcp \$c0,-1\(\$2\)
 74c:	ac f5 01 00 	swcp \$c5,1\(\$10\)
 750:	cc f8 02 00 	swcp \$c8,2\(\$12\)
 754:	1c fe ff ff 	swcp \$c14,-1\(\$1\)
 758:	3c fc 02 00 	swcp \$c12,2\(\$3\)

0000075c <lwcp16>:
 75c:	5d f8 ff ff 	lwcp \$c8,-1\(\$5\)
 760:	fd fc 01 00 	lwcp \$c12,1\(\$sp\)
 764:	0d f1 02 00 	lwcp \$c1,2\(\$0\)
 768:	dd f4 01 00 	lwcp \$c4,1\(\$tp\)
 76c:	bd f6 02 00 	lwcp \$c6,2\(\$11\)

00000770 <smcp16>:
 770:	ae f9 ff ff 	smcp \$c9,-1\(\$10\)
 774:	ee fe 01 00 	smcp \$c14,1\(\$gp\)
 778:	fe f3 02 00 	smcp \$c3,2\(\$sp\)
 77c:	8e ff fe ff 	smcp \$c15,-2\(\$8\)
 780:	de fd 01 00 	smcp \$c13,1\(\$tp\)

00000784 <lmcp16>:
 784:	ff f0 01 00 	lmcp \$c0,1\(\$sp\)
 788:	8f ff 01 00 	lmcp \$c15,1\(\$8\)
 78c:	8f f2 ff ff 	lmcp \$c2,-1\(\$8\)
 790:	8f fe 01 00 	lmcp \$c14,1\(\$8\)
 794:	af f1 ff ff 	lmcp \$c1,-1\(\$10\)

00000798 <sbcpa>:
 798:	f5 fe 02 00 	sbcpa \$c14,\(\$sp\+\),2
 79c:	45 f2 fe 00 	sbcpa \$c2,\(\$4\+\),-2
 7a0:	15 f8 00 00 	sbcpa \$c8,\(\$1\+\),0
 7a4:	35 fb 00 00 	sbcpa \$c11,\(\$3\+\),0
 7a8:	e5 f9 fe 00 	sbcpa \$c9,\(\$gp\+\),-2

000007ac <lbcpa>:
 7ac:	25 f7 fe 40 	lbcpa \$c7,\(\$2\+\),-2
 7b0:	f5 fc 02 40 	lbcpa \$c12,\(\$sp\+\),2
 7b4:	45 f5 fe 40 	lbcpa \$c5,\(\$4\+\),-2
 7b8:	45 f7 fe 40 	lbcpa \$c7,\(\$4\+\),-2
 7bc:	f5 f8 00 40 	lbcpa \$c8,\(\$sp\+\),0

000007c0 <shcpa>:
 7c0:	e5 f0 00 10 	shcpa \$c0,\(\$gp\+\),0
 7c4:	f5 fc 10 10 	shcpa \$c12,\(\$sp\+\),16
 7c8:	45 f1 04 10 	shcpa \$c1,\(\$4\+\),4
 7cc:	45 f5 e0 10 	shcpa \$c5,\(\$4\+\),-32
 7d0:	f5 f1 00 10 	shcpa \$c1,\(\$sp\+\),0

000007d4 <lhcpa>:
 7d4:	45 f4 00 50 	lhcpa \$c4,\(\$4\+\),0
 7d8:	55 f6 30 50 	lhcpa \$c6,\(\$5\+\),48
 7dc:	65 f3 cc 50 	lhcpa \$c3,\(\$6\+\),-52
 7e0:	65 f8 e8 50 	lhcpa \$c8,\(\$6\+\),-24
 7e4:	95 f0 00 50 	lhcpa \$c0,\(\$9\+\),0

000007e8 <swcpa>:
 7e8:	95 f1 10 20 	swcpa \$c1,\(\$9\+\),16
 7ec:	f5 f7 20 20 	swcpa \$c7,\(\$sp\+\),32
 7f0:	c5 f3 30 20 	swcpa \$c3,\(\$12\+\),48
 7f4:	95 fa 08 20 	swcpa \$c10,\(\$9\+\),8
 7f8:	85 fe 04 20 	swcpa \$c14,\(\$8\+\),4

000007fc <lwcpa>:
 7fc:	e5 f6 f8 60 	lwcpa \$c6,\(\$gp\+\),-8
 800:	75 f4 04 60 	lwcpa \$c4,\(\$7\+\),4
 804:	e5 fb f0 60 	lwcpa \$c11,\(\$gp\+\),-16
 808:	f5 fa e0 60 	lwcpa \$c10,\(\$sp\+\),-32
 80c:	25 f2 08 60 	lwcpa \$c2,\(\$2\+\),8

00000810 <smcpa>:
 810:	f5 fd f8 30 	smcpa \$c13,\(\$sp\+\),-8
 814:	75 f6 f8 30 	smcpa \$c6,\(\$7\+\),-8
 818:	35 f5 10 30 	smcpa \$c5,\(\$3\+\),16
 81c:	f5 fd 10 30 	smcpa \$c13,\(\$sp\+\),16
 820:	c5 f3 30 30 	smcpa \$c3,\(\$12\+\),48

00000824 <lmcpa>:
 824:	45 f9 00 70 	lmcpa \$c9,\(\$4\+\),0
 828:	f5 f3 f0 70 	lmcpa \$c3,\(\$sp\+\),-16
 82c:	d5 ff 08 70 	lmcpa \$c15,\(\$tp\+\),8
 830:	85 f8 f8 70 	lmcpa \$c8,\(\$8\+\),-8
 834:	95 fa 00 70 	lmcpa \$c10,\(\$9\+\),0

00000838 <sbcpm0>:
 838:	d5 fa 08 08 	sbcpm0 \$c10,\(\$tp\+\),8
 83c:	55 fd f8 08 	sbcpm0 \$c13,\(\$5\+\),-8
 840:	55 f4 f8 08 	sbcpm0 \$c4,\(\$5\+\),-8
 844:	d5 fa 10 08 	sbcpm0 \$c10,\(\$tp\+\),16
 848:	55 f4 e8 08 	sbcpm0 \$c4,\(\$5\+\),-24

0000084c <lbcpm0>:
 84c:	45 f0 00 48 	lbcpm0 \$c0,\(\$4\+\),0
 850:	75 f9 f8 48 	lbcpm0 \$c9,\(\$7\+\),-8
 854:	85 fc 18 48 	lbcpm0 \$c12,\(\$8\+\),24
 858:	c5 f8 10 48 	lbcpm0 \$c8,\(\$12\+\),16
 85c:	85 f7 10 48 	lbcpm0 \$c7,\(\$8\+\),16

00000860 <shcpm0>:
 860:	d5 f2 02 18 	shcpm0 \$c2,\(\$tp\+\),2
 864:	f5 f7 fe 18 	shcpm0 \$c7,\(\$sp\+\),-2
 868:	25 f8 02 18 	shcpm0 \$c8,\(\$2\+\),2
 86c:	55 fd 00 18 	shcpm0 \$c13,\(\$5\+\),0
 870:	e5 f3 08 18 	shcpm0 \$c3,\(\$gp\+\),8

00000874 <lhcpm0>:
 874:	45 f7 08 58 	lhcpm0 \$c7,\(\$4\+\),8
 878:	35 f3 fe 58 	lhcpm0 \$c3,\(\$3\+\),-2
 87c:	15 f3 00 58 	lhcpm0 \$c3,\(\$1\+\),0
 880:	e5 f2 00 58 	lhcpm0 \$c2,\(\$gp\+\),0
 884:	65 fc 02 58 	lhcpm0 \$c12,\(\$6\+\),2

00000888 <swcpm0>:
 888:	85 f8 20 28 	swcpm0 \$c8,\(\$8\+\),32
 88c:	f5 f9 00 28 	swcpm0 \$c9,\(\$sp\+\),0
 890:	25 f9 f0 28 	swcpm0 \$c9,\(\$2\+\),-16
 894:	e5 f0 30 28 	swcpm0 \$c0,\(\$gp\+\),48
 898:	15 ff 08 28 	swcpm0 \$c15,\(\$1\+\),8

0000089c <lwcpm0>:
 89c:	a5 fe fc 68 	lwcpm0 \$c14,\(\$10\+\),-4
 8a0:	f5 fb fc 68 	lwcpm0 \$c11,\(\$sp\+\),-4
 8a4:	75 f5 f8 68 	lwcpm0 \$c5,\(\$7\+\),-8
 8a8:	c5 f2 20 68 	lwcpm0 \$c2,\(\$12\+\),32
 8ac:	e5 f2 10 68 	lwcpm0 \$c2,\(\$gp\+\),16

000008b0 <smcpm0>:
 8b0:	c5 f1 08 38 	smcpm0 \$c1,\(\$12\+\),8
 8b4:	45 f8 f0 38 	smcpm0 \$c8,\(\$4\+\),-16
 8b8:	b5 fa 00 38 	smcpm0 \$c10,\(\$11\+\),0
 8bc:	35 f1 f0 38 	smcpm0 \$c1,\(\$3\+\),-16
 8c0:	f5 fb f8 38 	smcpm0 \$c11,\(\$sp\+\),-8

000008c4 <lmcpm0>:
 8c4:	a5 fe 00 78 	lmcpm0 \$c14,\(\$10\+\),0
 8c8:	f5 f6 f0 78 	lmcpm0 \$c6,\(\$sp\+\),-16
 8cc:	15 fd 08 78 	lmcpm0 \$c13,\(\$1\+\),8
 8d0:	d5 fa e8 78 	lmcpm0 \$c10,\(\$tp\+\),-24
 8d4:	e5 f7 e8 78 	lmcpm0 \$c7,\(\$gp\+\),-24

000008d8 <sbcpm1>:
 8d8:	85 f9 00 0c 	sbcpm1 \$c9,\(\$8\+\),0
 8dc:	c5 f7 e8 0c 	sbcpm1 \$c7,\(\$12\+\),-24
 8e0:	55 ff e8 0c 	sbcpm1 \$c15,\(\$5\+\),-24
 8e4:	d5 f5 10 0c 	sbcpm1 \$c5,\(\$tp\+\),16
 8e8:	15 f6 80 0c 	sbcpm1 \$c6,\(\$1\+\),-128

000008ec <lbcpm1>:
 8ec:	e5 f6 02 4c 	lbcpm1 \$c6,\(\$gp\+\),2
 8f0:	d5 f7 fe 4c 	lbcpm1 \$c7,\(\$tp\+\),-2
 8f4:	d5 f4 01 4c 	lbcpm1 \$c4,\(\$tp\+\),1
 8f8:	25 fc fe 4c 	lbcpm1 \$c12,\(\$2\+\),-2
 8fc:	75 fb 01 4c 	lbcpm1 \$c11,\(\$7\+\),1

00000900 <shcpm1>:
 900:	85 f4 18 1c 	shcpm1 \$c4,\(\$8\+\),24
 904:	65 fb f0 1c 	shcpm1 \$c11,\(\$6\+\),-16
 908:	85 f7 08 1c 	shcpm1 \$c7,\(\$8\+\),8
 90c:	c5 f5 10 1c 	shcpm1 \$c5,\(\$12\+\),16
 910:	85 f0 e0 1c 	shcpm1 \$c0,\(\$8\+\),-32

00000914 <lhcpm1>:
 914:	05 fb 00 5c 	lhcpm1 \$c11,\(\$0\+\),0
 918:	d5 f7 fe 5c 	lhcpm1 \$c7,\(\$tp\+\),-2
 91c:	85 fa 08 5c 	lhcpm1 \$c10,\(\$8\+\),8
 920:	d5 f3 00 5c 	lhcpm1 \$c3,\(\$tp\+\),0
 924:	65 f9 02 5c 	lhcpm1 \$c9,\(\$6\+\),2

00000928 <swcpm1>:
 928:	85 f9 18 2c 	swcpm1 \$c9,\(\$8\+\),24
 92c:	e5 f9 00 2c 	swcpm1 \$c9,\(\$gp\+\),0
 930:	85 f9 10 2c 	swcpm1 \$c9,\(\$8\+\),16
 934:	15 fe 00 2c 	swcpm1 \$c14,\(\$1\+\),0
 938:	f5 f2 08 2c 	swcpm1 \$c2,\(\$sp\+\),8

0000093c <lwcpm1>:
 93c:	85 f8 00 6c 	lwcpm1 \$c8,\(\$8\+\),0
 940:	e5 f3 f0 6c 	lwcpm1 \$c3,\(\$gp\+\),-16
 944:	65 f7 f8 6c 	lwcpm1 \$c7,\(\$6\+\),-8
 948:	85 fe e8 6c 	lwcpm1 \$c14,\(\$8\+\),-24
 94c:	85 f3 18 6c 	lwcpm1 \$c3,\(\$8\+\),24

00000950 <smcpm1>:
 950:	45 fa 00 3c 	smcpm1 \$c10,\(\$4\+\),0
 954:	f5 f6 f0 3c 	smcpm1 \$c6,\(\$sp\+\),-16
 958:	75 fd e8 3c 	smcpm1 \$c13,\(\$7\+\),-24
 95c:	e5 f3 f8 3c 	smcpm1 \$c3,\(\$gp\+\),-8
 960:	25 f0 08 3c 	smcpm1 \$c0,\(\$2\+\),8

00000964 <lmcpm1>:
 964:	15 fc 00 7c 	lmcpm1 \$c12,\(\$1\+\),0
 968:	65 f0 08 7c 	lmcpm1 \$c0,\(\$6\+\),8
 96c:	25 f6 f8 7c 	lmcpm1 \$c6,\(\$2\+\),-8
 970:	e5 fc f0 7c 	lmcpm1 \$c12,\(\$gp\+\),-16
 974:	f5 fe 30 7c 	lmcpm1 \$c14,\(\$sp\+\),48

00000... <bcpeq>:
 ...:	44 d8 00 00 	bcpeq 0x4,... <bcpeq>
 ...:	04 d8 ff ff 	bcpeq 0x0,... <bcpeq\+0x2>
 ...:	44 d8 ff ff 	bcpeq 0x4,... <bcpeq\+0x6>
 ...:	14 d8 01 00 	bcpeq 0x1,... <bcpeq\+0xe>
 ...:	24 d8 01 00 	bcpeq 0x2,... <bcpeq\+0x12>

00000... <bcpne>:
 ...:	25 d8 00 00 	bcpne 0x2,... <bcpne>
 ...:	45 d8 00 00 	bcpne 0x4,... <bcpne\+0x4>
 ...:	15 d8 00 00 	bcpne 0x1,... <bcpne\+0x8>
 ...:	45 d8 00 00 	bcpne 0x4,... <bcpne\+0xc>
 ...:	15 d8 01 00 	bcpne 0x1,... <bcpne\+0x12>

00000... <bcpat>:
 ...:	16 d8 ff ff 	bcpat 0x1,... <bcpne\+0x12>
 ...:	06 d8 01 00 	bcpat 0x0,... <bcpat\+0x6>
 ...:	06 d8 ff ff 	bcpat 0x0,... <bcpat\+0x6>
 ...:	26 d8 00 00 	bcpat 0x2,... <bcpat\+0xc>
 ...:	16 d8 ff ff 	bcpat 0x1,... <bcpat\+0xe>

00000... <bcpaf>:
 ...:	47 d8 00 00 	bcpaf 0x4,... <bcpaf>
 ...:	37 d8 00 00 	bcpaf 0x3,... <bcpaf\+0x4>
 ...:	47 d8 00 00 	bcpaf 0x4,... <bcpaf\+0x8>
 ...:	17 d8 01 00 	bcpaf 0x1,... <bcpaf\+0xe>
 ...:	47 d8 01 00 	bcpaf 0x4,... <bcpaf\+0x12>

00000... <synccp>:
 ...:	21 70       	synccp

00000... <jsrv>:
 ...:	bf 18       	jsrv \$11
 ...:	5f 18       	jsrv \$5
 ...:	af 18       	jsrv \$10
 ...:	cf 18       	jsrv \$12
 ...:	af 18       	jsrv \$10

00000... <bsrv>:
 ...:	fb df ff ff 	bsrv ... <jsrv\+0x8>
 ...:	fb df ff ff 	bsrv ... <bsrv\+0x2>
 ...:	fb df ff ff 	bsrv ... <bsrv\+0x6>
 ...:	1b d8 00 00 	bsrv ... <bsrv\+0xe>
 ...:	0b d8 00 00 	bsrv ... <bsrv\+0x10>

00000... <case106341>:
 ...:	78 7a       	stc \$10,\$hi
 ...:	8a 70       	ldc \$0,\$lo

00000... <case106821>:
 ...:	08 00       	sb \$0,\(\$0\)
 ...:	09 00       	sh \$0,\(\$0\)
 ...:	0a 00       	sw \$0,\(\$0\)
 ...:	0c 00       	lb \$0,\(\$0\)
 ...:	0d 00       	lh \$0,\(\$0\)
 ...:	0e 00       	lw \$0,\(\$0\)
 ...:	0b 00       	lbu \$0,\(\$0\)
 ...:	0f 00       	lhu \$0,\(\$0\)
 ...:	08 00       	sb \$0,\(\$0\)
 ...:	08 00       	sb \$0,\(\$0\)
 ...:	08 00       	sb \$0,\(\$0\)
 ...:	08 00       	sb \$0,\(\$0\)
 ...:	08 00       	sb \$0,\(\$0\)
 ...:	08 00       	sb \$0,\(\$0\)
 ...:	09 00       	sh \$0,\(\$0\)
 ...:	09 00       	sh \$0,\(\$0\)
 ...:	09 00       	sh \$0,\(\$0\)
 ...:	09 00       	sh \$0,\(\$0\)
 ...:	09 00       	sh \$0,\(\$0\)
 ...:	09 00       	sh \$0,\(\$0\)
 ...:	0a 00       	sw \$0,\(\$0\)
 ...:	0a 00       	sw \$0,\(\$0\)
 ...:	0a 00       	sw \$0,\(\$0\)
 ...:	0a 00       	sw \$0,\(\$0\)
 ...:	0a 00       	sw \$0,\(\$0\)
 ...:	0a 00       	sw \$0,\(\$0\)
 ...:	0c 00       	lb \$0,\(\$0\)
 ...:	0c 00       	lb \$0,\(\$0\)
 ...:	0c 00       	lb \$0,\(\$0\)
 ...:	0c 00       	lb \$0,\(\$0\)
 ...:	0c 00       	lb \$0,\(\$0\)
 ...:	0c 00       	lb \$0,\(\$0\)
 ...:	0d 00       	lh \$0,\(\$0\)
 ...:	0d 00       	lh \$0,\(\$0\)
 ...:	0d 00       	lh \$0,\(\$0\)
 ...:	0d 00       	lh \$0,\(\$0\)
 ...:	0d 00       	lh \$0,\(\$0\)
 ...:	0d 00       	lh \$0,\(\$0\)
 ...:	0e 00       	lw \$0,\(\$0\)
 ...:	0e 00       	lw \$0,\(\$0\)
 ...:	0e 00       	lw \$0,\(\$0\)
 ...:	0e 00       	lw \$0,\(\$0\)
 ...:	0e 00       	lw \$0,\(\$0\)
 ...:	0e 00       	lw \$0,\(\$0\)
 ...:	0b 00       	lbu \$0,\(\$0\)
 ...:	0b 00       	lbu \$0,\(\$0\)
 ...:	0b 00       	lbu \$0,\(\$0\)
 ...:	0b 00       	lbu \$0,\(\$0\)
 ...:	0b 00       	lbu \$0,\(\$0\)
 ...:	0b 00       	lbu \$0,\(\$0\)
 ...:	0f 00       	lhu \$0,\(\$0\)
 ...:	0f 00       	lhu \$0,\(\$0\)
 ...:	0f 00       	lhu \$0,\(\$0\)
 ...:	0f 00       	lhu \$0,\(\$0\)
 ...:	0f 00       	lhu \$0,\(\$0\)
 ...:	0f 00       	lhu \$0,\(\$0\)
 ...:	08 c0 01 00 	sb \$0,1\(\$0\)
 ...:	08 c0 01 00 	sb \$0,1\(\$0\)
 ...:	08 c0 00 00 	sb \$0,0\(\$0\)
 ...:	08 c0 00 00 	sb \$0,0\(\$0\)
 ...:	08 c0 01 00 	sb \$0,1\(\$0\)
 ...:	08 c0 01 00 	sb \$0,1\(\$0\)
 ...:	09 c0 01 00 	sh \$0,1\(\$0\)
 ...:	09 c0 01 00 	sh \$0,1\(\$0\)
 ...:	09 c0 00 00 	sh \$0,0\(\$0\)
 ...:	09 c0 00 00 	sh \$0,0\(\$0\)
 ...:	09 c0 01 00 	sh \$0,1\(\$0\)
 ...:	09 c0 01 00 	sh \$0,1\(\$0\)
 ...:	0a c0 01 00 	sw \$0,1\(\$0\)
 ...:	0a c0 01 00 	sw \$0,1\(\$0\)
 ...:	0a c0 00 00 	sw \$0,0\(\$0\)
 ...:	0a c0 00 00 	sw \$0,0\(\$0\)
 ...:	0a c0 01 00 	sw \$0,1\(\$0\)
 ...:	0a c0 01 00 	sw \$0,1\(\$0\)
 ...:	0c c0 01 00 	lb \$0,1\(\$0\)
 ...:	0c c0 01 00 	lb \$0,1\(\$0\)
 ...:	0c c0 00 00 	lb \$0,0\(\$0\)
 ...:	0c c0 00 00 	lb \$0,0\(\$0\)
 ...:	0c c0 01 00 	lb \$0,1\(\$0\)
 ...:	0c c0 01 00 	lb \$0,1\(\$0\)
 ...:	0d c0 01 00 	lh \$0,1\(\$0\)
 ...:	0d c0 01 00 	lh \$0,1\(\$0\)
 ...:	0d c0 00 00 	lh \$0,0\(\$0\)
 ...:	0d c0 00 00 	lh \$0,0\(\$0\)
 ...:	0d c0 01 00 	lh \$0,1\(\$0\)
 ...:	0d c0 01 00 	lh \$0,1\(\$0\)
 ...:	0e c0 01 00 	lw \$0,1\(\$0\)
 ...:	0e c0 01 00 	lw \$0,1\(\$0\)
 ...:	0e c0 00 00 	lw \$0,0\(\$0\)
 ...:	0e c0 00 00 	lw \$0,0\(\$0\)
 ...:	0e c0 01 00 	lw \$0,1\(\$0\)
 ...:	0e c0 01 00 	lw \$0,1\(\$0\)
 ...:	0b c0 01 00 	lbu \$0,1\(\$0\)
 ...:	0b c0 01 00 	lbu \$0,1\(\$0\)
 ...:	0b c0 00 00 	lbu \$0,0\(\$0\)
 ...:	0b c0 00 00 	lbu \$0,0\(\$0\)
 ...:	0b c0 01 00 	lbu \$0,1\(\$0\)
 ...:	0b c0 01 00 	lbu \$0,1\(\$0\)
 ...:	0f c0 01 00 	lhu \$0,1\(\$0\)
 ...:	0f c0 01 00 	lhu \$0,1\(\$0\)
 ...:	0f c0 00 00 	lhu \$0,0\(\$0\)
 ...:	0f c0 00 00 	lhu \$0,0\(\$0\)
 ...:	0f c0 01 00 	lhu \$0,1\(\$0\)
 ...:	0f c0 01 00 	lhu \$0,1\(\$0\)
 ...:	08 c0 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	08 c0 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	08 c0 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	08 c0 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	09 c0 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	09 c0 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	09 c0 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	09 c0 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	0a c0 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	0a c0 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	0a c0 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	0a c0 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	0c c0 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	0c c0 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	0c c0 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	0c c0 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	0d c0 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	0d c0 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	0d c0 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	0d c0 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	0e c0 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	0e c0 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	0e c0 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	0e c0 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	0b c0 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	0b c0 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	0b c0 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	0b c0 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	0f c0 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	0f c0 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	0f c0 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	0f c0 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
