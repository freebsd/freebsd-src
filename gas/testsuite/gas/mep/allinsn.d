#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section .text:

00000000 <sb>:
   0:	07 88       	sb \$7,\(\$8\)
   2:	05 98       	sb \$5,\(\$9\)
   4:	07 e8       	sb \$7,\(\$gp\)
   6:	0e 88       	sb \$gp,\(\$8\)
   8:	0f e8       	sb \$sp,\(\$gp\)

0000000a <sh>:
   a:	03 89       	sh \$3,\(\$8\)
   c:	0c 19       	sh \$12,\(\$1\)
   e:	0d 29       	sh \$tp,\(\$2\)
  10:	02 89       	sh \$2,\(\$8\)
  12:	0c a9       	sh \$12,\(\$10\)

00000014 <sw>:
  14:	0b 0a       	sw \$11,\(\$0\)
  16:	03 7a       	sw \$3,\(\$7\)
  18:	0d ea       	sw \$tp,\(\$gp\)
  1a:	08 9a       	sw \$8,\(\$9\)
  1c:	0e 8a       	sw \$gp,\(\$8\)

0000001e <lb>:
  1e:	0c bc       	lb \$12,\(\$11\)
  20:	09 2c       	lb \$9,\(\$2\)
  22:	08 bc       	lb \$8,\(\$11\)
  24:	0e 2c       	lb \$gp,\(\$2\)
  26:	02 cc       	lb \$2,\(\$12\)

00000028 <lh>:
  28:	0f 8d       	lh \$sp,\(\$8\)
  2a:	03 ad       	lh \$3,\(\$10\)
  2c:	09 fd       	lh \$9,\(\$sp\)
  2e:	06 fd       	lh \$6,\(\$sp\)
  30:	0f bd       	lh \$sp,\(\$11\)

00000032 <lw>:
  32:	0c ae       	lw \$12,\(\$10\)
  34:	09 de       	lw \$9,\(\$tp\)
  36:	0c ee       	lw \$12,\(\$gp\)
  38:	0c be       	lw \$12,\(\$11\)
  3a:	0d ae       	lw \$tp,\(\$10\)

0000003c <lbu>:
  3c:	0e eb       	lbu \$gp,\(\$gp\)
  3e:	0c 8b       	lbu \$12,\(\$8\)
  40:	0e 1b       	lbu \$gp,\(\$1\)
  42:	08 cb       	lbu \$8,\(\$12\)
  44:	0c 1b       	lbu \$12,\(\$1\)

00000046 <lhu>:
  46:	0f 4f       	lhu \$sp,\(\$4\)
  48:	0e 4f       	lhu \$gp,\(\$4\)
  4a:	05 4f       	lhu \$5,\(\$4\)
  4c:	0f df       	lhu \$sp,\(\$tp\)
  4e:	04 ff       	lhu \$4,\(\$sp\)

00000050 <sw_sp>:
  50:	c9 8a 00 03 	sw \$9,3\(\$8\)
  54:	ca 5a 00 04 	sw \$10,4\(\$5\)
  58:	c0 ea 00 03 	sw \$0,3\(\$gp\)
  5c:	c0 8a 00 02 	sw \$0,2\(\$8\)
  60:	cf 8a 00 01 	sw \$sp,1\(\$8\)

00000064 <lw_sp>:
  64:	cd 5e 00 01 	lw \$tp,1\(\$5\)
  68:	cf 0e 00 01 	lw \$sp,1\(\$0\)
  6c:	c0 ce 00 04 	lw \$0,4\(\$12\)
  70:	cb de 00 01 	lw \$11,1\(\$tp\)
  74:	c9 4e 00 03 	lw \$9,3\(\$4\)

00000078 <sb_tp>:
  78:	c5 18 00 01 	sb \$5,1\(\$1\)
  7c:	ca 98 00 01 	sb \$10,1\(\$9\)
  80:	c5 38 00 03 	sb \$5,3\(\$3\)
  84:	c5 38 00 01 	sb \$5,1\(\$3\)
  88:	ca 48 00 04 	sb \$10,4\(\$4\)

0000008c <sh_tp>:
  8c:	c3 09 00 01 	sh \$3,1\(\$0\)
  90:	cd 99 00 01 	sh \$tp,1\(\$9\)
  94:	c9 a9 00 04 	sh \$9,4\(\$10\)
  98:	cf e9 00 03 	sh \$sp,3\(\$gp\)
  9c:	ce 99 00 04 	sh \$gp,4\(\$9\)

000000a0 <sw_tp>:
  a0:	c6 da 00 02 	sw \$6,2\(\$tp\)
  a4:	c6 fa 00 01 	sw \$6,1\(\$sp\)
  a8:	c2 3a 00 02 	sw \$2,2\(\$3\)
  ac:	c6 ca 00 02 	sw \$6,2\(\$12\)
  b0:	c3 ba 00 01 	sw \$3,1\(\$11\)

000000b4 <lb_tp>:
  b4:	cd bc 00 04 	lb \$tp,4\(\$11\)
  b8:	cd 8c 00 04 	lb \$tp,4\(\$8\)
  bc:	c5 5c 00 04 	lb \$5,4\(\$5\)
  c0:	cf ec 00 02 	lb \$sp,2\(\$gp\)
  c4:	c3 3c 00 02 	lb \$3,2\(\$3\)

000000c8 <lh_tp>:
  c8:	c7 8d 00 02 	lh \$7,2\(\$8\)
  cc:	c4 8d 00 03 	lh \$4,3\(\$8\)
  d0:	ce fd 00 01 	lh \$gp,1\(\$sp\)
  d4:	c9 0d 00 01 	lh \$9,1\(\$0\)
  d8:	cd 0d 00 02 	lh \$tp,2\(\$0\)

000000dc <lw_tp>:
  dc:	48 07       	lw \$8,0x4\(\$sp\)
  de:	cb 9e 00 04 	lw \$11,4\(\$9\)
  e2:	ce 2e 00 01 	lw \$gp,1\(\$2\)
  e6:	c9 ee 00 02 	lw \$9,2\(\$gp\)
  ea:	c8 ce 00 01 	lw \$8,1\(\$12\)

000000ee <lbu_tp>:
  ee:	cc 9b 00 01 	lbu \$12,1\(\$9\)
  f2:	cb 9b 00 01 	lbu \$11,1\(\$9\)
  f6:	ce 8b 00 03 	lbu \$gp,3\(\$8\)
  fa:	c0 fb 00 02 	lbu \$0,2\(\$sp\)
  fe:	cd bb 00 01 	lbu \$tp,1\(\$11\)

00000102 <lhu_tp>:
 102:	ce af 00 02 	lhu \$gp,2\(\$10\)
 106:	cb 8f 00 01 	lhu \$11,1\(\$8\)
 10a:	c1 0f 00 01 	lhu \$1,1\(\$0\)
 10e:	c7 ff 00 02 	lhu \$7,2\(\$sp\)
 112:	8b 83       	lhu \$3,0x2\(\$tp\)

00000114 <sb16>:
 114:	c7 b8 ff ff 	sb \$7,-1\(\$11\)
 118:	cd e8 00 01 	sb \$tp,1\(\$gp\)
 11c:	c3 e8 00 01 	sb \$3,1\(\$gp\)
 120:	ce 68 00 02 	sb \$gp,2\(\$6\)
 124:	ce 78 00 01 	sb \$gp,1\(\$7\)

00000128 <sh16>:
 128:	cc 49 ff ff 	sh \$12,-1\(\$4\)
 12c:	cf 19 00 01 	sh \$sp,1\(\$1\)
 130:	c2 c9 ff fe 	sh \$2,-2\(\$12\)
 134:	c9 b9 00 02 	sh \$9,2\(\$11\)
 138:	c9 c9 ff fe 	sh \$9,-2\(\$12\)

0000013c <sw16>:
 13c:	cb ea ff ff 	sw \$11,-1\(\$gp\)
 140:	44 06       	sw \$4,0x4\(\$sp\)
 142:	c2 3a ff fe 	sw \$2,-2\(\$3\)
 146:	c6 2a ff ff 	sw \$6,-1\(\$2\)
 14a:	c8 da ff fe 	sw \$8,-2\(\$tp\)

0000014e <lb16>:
 14e:	ca 2c ff fe 	lb \$10,-2\(\$2\)
 152:	c3 bc ff fe 	lb \$3,-2\(\$11\)
 156:	cc 5c 00 01 	lb \$12,1\(\$5\)
 15a:	c5 5c 00 01 	lb \$5,1\(\$5\)
 15e:	cb dc 00 02 	lb \$11,2\(\$tp\)

00000162 <lh16>:
 162:	cf bd ff ff 	lh \$sp,-1\(\$11\)
 166:	cd bd ff fe 	lh \$tp,-2\(\$11\)
 16a:	c2 ad 00 01 	lh \$2,1\(\$10\)
 16e:	c8 7d ff ff 	lh \$8,-1\(\$7\)
 172:	ce bd ff ff 	lh \$gp,-1\(\$11\)

00000176 <lw16>:
 176:	c0 5e ff ff 	lw \$0,-1\(\$5\)
 17a:	cc 7e ff fe 	lw \$12,-2\(\$7\)
 17e:	c1 3e ff fe 	lw \$1,-2\(\$3\)
 182:	c1 7e 00 02 	lw \$1,2\(\$7\)
 186:	c4 8e 00 01 	lw \$4,1\(\$8\)

0000018a <lbu16>:
 18a:	cc 4b ff ff 	lbu \$12,-1\(\$4\)
 18e:	ce bb 00 01 	lbu \$gp,1\(\$11\)
 192:	c1 db ff ff 	lbu \$1,-1\(\$tp\)
 196:	c9 db ff ff 	lbu \$9,-1\(\$tp\)
 19a:	c8 fb 00 01 	lbu \$8,1\(\$sp\)

0000019e <lhu16>:
 19e:	cd ff ff ff 	lhu \$tp,-1\(\$sp\)
 1a2:	ce 8f 00 02 	lhu \$gp,2\(\$8\)
 1a6:	cf cf ff ff 	lhu \$sp,-1\(\$12\)
 1aa:	c3 0f ff ff 	lhu \$3,-1\(\$0\)
 1ae:	c3 cf ff fe 	lhu \$3,-2\(\$12\)

000001b2 <sw24>:
 1b2:	eb 06 00 00 	sw \$11,\(0x4\)
 1b6:	ef 06 00 00 	sw \$sp,\(0x4\)
 1ba:	e7 0a 00 00 	sw \$7,\(0x8\)
 1be:	ea 12 00 00 	sw \$10,\(0x10\)
 1c2:	e8 a2 00 00 	sw \$8,\(0xa0\)

000001c6 <lw24>:
 1c6:	e4 07 00 00 	lw \$4,\(0x4\)
 1ca:	ef 07 00 00 	lw \$sp,\(0x4\)
 1ce:	e4 13 00 00 	lw \$4,\(0x10\)
 1d2:	e8 03 00 00 	lw \$8,\(0x0\)
 1d6:	ed 0b 00 00 	lw \$tp,\(0x8\)

000001da <extb>:
 1da:	1d 0d       	extb \$tp
 1dc:	1d 0d       	extb \$tp
 1de:	16 0d       	extb \$6
 1e0:	1e 0d       	extb \$gp
 1e2:	1a 0d       	extb \$10

000001e4 <exth>:
 1e4:	1f 2d       	exth \$sp
 1e6:	12 2d       	exth \$2
 1e8:	15 2d       	exth \$5
 1ea:	1a 2d       	exth \$10
 1ec:	14 2d       	exth \$4

000001ee <extub>:
 1ee:	12 8d       	extub \$2
 1f0:	1d 8d       	extub \$tp
 1f2:	13 8d       	extub \$3
 1f4:	19 8d       	extub \$9
 1f6:	1e 8d       	extub \$gp

000001f8 <extuh>:
 1f8:	18 ad       	extuh \$8
 1fa:	18 ad       	extuh \$8
 1fc:	14 ad       	extuh \$4
 1fe:	10 ad       	extuh \$0
 200:	10 ad       	extuh \$0

00000202 <ssarb>:
 202:	12 8c       	ssarb 2\(\$8\)
 204:	12 dc       	ssarb 2\(\$tp\)
 206:	11 dc       	ssarb 1\(\$tp\)
 208:	12 5c       	ssarb 2\(\$5\)
 20a:	10 9c       	ssarb 0\(\$9\)

0000020c <mov>:
 20c:	02 30       	mov \$2,\$3
 20e:	03 b0       	mov \$3,\$11
 210:	0f a0       	mov \$sp,\$10
 212:	0f 00       	mov \$sp,\$0
 214:	03 d0       	mov \$3,\$tp

00000216 <movi8>:
 216:	5b ff       	mov \$11,-1
 218:	56 02       	mov \$6,2
 21a:	5f ff       	mov \$sp,-1
 21c:	5f 01       	mov \$sp,1
 21e:	5e ff       	mov \$gp,-1

00000220 <movi16>:
 220:	5f 00       	mov \$sp,0
 222:	50 02       	mov \$0,2
 224:	58 ff       	mov \$8,-1
 226:	5c 01       	mov \$12,1
 228:	57 ff       	mov \$7,-1

0000022a <movu24>:
 22a:	d2 01 00 00 	movu \$2,0x1
 22e:	ca 11 00 04 	movu \$10,0x4
 232:	c9 11 00 00 	movu \$9,0x0
 236:	d4 03 00 00 	movu \$4,0x3
 23a:	ce 11 00 01 	movu \$gp,0x1

0000023e <movu16>:
 23e:	cf 11 00 01 	movu \$sp,0x1
 242:	d6 03 00 00 	movu \$6,0x3
 246:	d0 03 00 00 	movu \$0,0x3
 24a:	ce 11 00 03 	movu \$gp,0x3
 24e:	ca 11 00 02 	movu \$10,0x2

00000252 <movh>:
 252:	c8 21 00 02 	movh \$8,0x2
 256:	cd 21 00 01 	movh \$tp,0x1
 25a:	ce 21 00 02 	movh \$gp,0x2
 25e:	cc 21 00 00 	movh \$12,0x0
 262:	cb 21 00 02 	movh \$11,0x2

00000266 <add3>:
 266:	9b 36       	add3 \$6,\$11,\$3
 268:	9d 5e       	add3 \$gp,\$tp,\$5
 26a:	9b 73       	add3 \$3,\$11,\$7
 26c:	9e dd       	add3 \$tp,\$gp,\$tp
 26e:	9e 80       	add3 \$0,\$gp,\$8

00000270 <add>:
 270:	6c 08       	add \$12,2
 272:	6c fc       	add \$12,-1
 274:	64 04       	add \$4,1
 276:	66 04       	add \$6,1
 278:	66 08       	add \$6,2

0000027a <add3i>:
 27a:	4b 04       	add3 \$11,\$sp,0x4
 27c:	c4 f0 00 01 	add3 \$4,\$sp,1
 280:	40 00       	add3 \$0,\$sp,0x0
 282:	cd f0 00 03 	add3 \$tp,\$sp,3
 286:	4b 00       	add3 \$11,\$sp,0x0

00000288 <advck3>:
 288:	0e a7       	advck3 \$0,\$gp,\$10
 28a:	0d 07       	advck3 \$0,\$tp,\$0
 28c:	0e d7       	advck3 \$0,\$gp,\$tp
 28e:	07 87       	advck3 \$0,\$7,\$8
 290:	01 27       	advck3 \$0,\$1,\$2

00000292 <sub>:
 292:	08 e4       	sub \$8,\$gp
 294:	01 94       	sub \$1,\$9
 296:	0d 74       	sub \$tp,\$7
 298:	0f 34       	sub \$sp,\$3
 29a:	02 74       	sub \$2,\$7

0000029c <sbvck3>:
 29c:	03 e5       	sbvck3 \$0,\$3,\$gp
 29e:	03 75       	sbvck3 \$0,\$3,\$7
 2a0:	0a a5       	sbvck3 \$0,\$10,\$10
 2a2:	04 d5       	sbvck3 \$0,\$4,\$tp
 2a4:	0a f5       	sbvck3 \$0,\$10,\$sp

000002a6 <neg>:
 2a6:	0e 71       	neg \$gp,\$7
 2a8:	01 71       	neg \$1,\$7
 2aa:	02 b1       	neg \$2,\$11
 2ac:	0d 81       	neg \$tp,\$8
 2ae:	0e d1       	neg \$gp,\$tp

000002b0 <slt3>:
 2b0:	0e 82       	slt3 \$0,\$gp,\$8
 2b2:	04 d2       	slt3 \$0,\$4,\$tp
 2b4:	0a e2       	slt3 \$0,\$10,\$gp
 2b6:	0e 52       	slt3 \$0,\$gp,\$5
 2b8:	03 c2       	slt3 \$0,\$3,\$12

000002ba <sltu3>:
 2ba:	02 83       	sltu3 \$0,\$2,\$8
 2bc:	0e b3       	sltu3 \$0,\$gp,\$11
 2be:	02 d3       	sltu3 \$0,\$2,\$tp
 2c0:	09 83       	sltu3 \$0,\$9,\$8
 2c2:	06 93       	sltu3 \$0,\$6,\$9

000002c4 <slt3i>:
 2c4:	66 11       	slt3 \$0,\$6,0x2
 2c6:	6b 09       	slt3 \$0,\$11,0x1
 2c8:	6f 01       	slt3 \$0,\$sp,0x0
 2ca:	63 01       	slt3 \$0,\$3,0x0
 2cc:	6d 01       	slt3 \$0,\$tp,0x0

000002ce <sltu3i>:
 2ce:	6e 25       	sltu3 \$0,\$gp,0x4
 2d0:	6d 1d       	sltu3 \$0,\$tp,0x3
 2d2:	63 0d       	sltu3 \$0,\$3,0x1
 2d4:	6c 05       	sltu3 \$0,\$12,0x0
 2d6:	61 1d       	sltu3 \$0,\$1,0x3

000002d8 <sl1ad3>:
 2d8:	28 e6       	sl1ad3 \$0,\$8,\$gp
 2da:	24 26       	sl1ad3 \$0,\$4,\$2
 2dc:	2f c6       	sl1ad3 \$0,\$sp,\$12
 2de:	29 16       	sl1ad3 \$0,\$9,\$1
 2e0:	28 26       	sl1ad3 \$0,\$8,\$2

000002e2 <sl2ad3>:
 2e2:	28 d7       	sl2ad3 \$0,\$8,\$tp
 2e4:	22 37       	sl2ad3 \$0,\$2,\$3
 2e6:	28 97       	sl2ad3 \$0,\$8,\$9
 2e8:	27 c7       	sl2ad3 \$0,\$7,\$12
 2ea:	24 c7       	sl2ad3 \$0,\$4,\$12

000002ec <add3x>:
 2ec:	cd b0 00 01 	add3 \$tp,\$11,1
 2f0:	cd 40 ff ff 	add3 \$tp,\$4,-1
 2f4:	c2 d0 00 01 	add3 \$2,\$tp,1
 2f8:	c3 e0 00 01 	add3 \$3,\$gp,1
 2fc:	ca f0 00 02 	add3 \$10,\$sp,2

00000300 <slt3x>:
 300:	c8 12 ff ff 	slt3 \$8,\$1,-1
 304:	c0 32 ff fe 	slt3 \$0,\$3,-2
 308:	c9 f2 ff ff 	slt3 \$9,\$sp,-1
 30c:	c3 82 00 02 	slt3 \$3,\$8,2
 310:	cd e2 00 00 	slt3 \$tp,\$gp,0

00000314 <sltu3x>:
 314:	cf b3 00 02 	sltu3 \$sp,\$11,0x2
 318:	c6 03 00 01 	sltu3 \$6,\$0,0x1
 31c:	c9 b3 00 03 	sltu3 \$9,\$11,0x3
 320:	64 05       	sltu3 \$0,\$4,0x0
 322:	cd e3 00 04 	sltu3 \$tp,\$gp,0x4

00000326 <or>:
 326:	1f e0       	or \$sp,\$gp
 328:	18 30       	or \$8,\$3
 32a:	10 f0       	or \$0,\$sp
 32c:	1d 00       	or \$tp,\$0
 32e:	18 60       	or \$8,\$6

00000330 <and>:
 330:	1f f1       	and \$sp,\$sp
 332:	16 e1       	and \$6,\$gp
 334:	14 21       	and \$4,\$2
 336:	15 81       	and \$5,\$8
 338:	17 e1       	and \$7,\$gp

0000033a <xor>:
 33a:	11 c2       	xor \$1,\$12
 33c:	1c d2       	xor \$12,\$tp
 33e:	1a 82       	xor \$10,\$8
 340:	1f b2       	xor \$sp,\$11
 342:	1c 82       	xor \$12,\$8

00000344 <nor>:
 344:	19 53       	nor \$9,\$5
 346:	18 23       	nor \$8,\$2
 348:	1f 93       	nor \$sp,\$9
 34a:	15 f3       	nor \$5,\$sp
 34c:	1f e3       	nor \$sp,\$gp

0000034e <or3>:
 34e:	cd f4 00 02 	or3 \$tp,\$sp,0x2
 352:	cf d4 00 03 	or3 \$sp,\$tp,0x3
 356:	c0 a4 00 04 	or3 \$0,\$10,0x4
 35a:	c9 f4 00 03 	or3 \$9,\$sp,0x3
 35e:	c9 f4 00 00 	or3 \$9,\$sp,0x0

00000362 <and3>:
 362:	c5 85 00 01 	and3 \$5,\$8,0x1
 366:	cb e5 00 03 	and3 \$11,\$gp,0x3
 36a:	c6 05 00 00 	and3 \$6,\$0,0x0
 36e:	cf f5 00 00 	and3 \$sp,\$sp,0x0
 372:	c1 a5 00 03 	and3 \$1,\$10,0x3

00000376 <xor3>:
 376:	c0 06 00 02 	xor3 \$0,\$0,0x2
 37a:	cf 66 00 00 	xor3 \$sp,\$6,0x0
 37e:	cd 56 00 00 	xor3 \$tp,\$5,0x0
 382:	cf 76 00 00 	xor3 \$sp,\$7,0x0
 386:	cf f6 00 02 	xor3 \$sp,\$sp,0x2

0000038a <sra>:
 38a:	24 1d       	sra \$4,\$1
 38c:	28 fd       	sra \$8,\$sp
 38e:	21 1d       	sra \$1,\$1
 390:	20 5d       	sra \$0,\$5
 392:	29 1d       	sra \$9,\$1

00000394 <srl>:
 394:	22 bc       	srl \$2,\$11
 396:	2f 7c       	srl \$sp,\$7
 398:	21 7c       	srl \$1,\$7
 39a:	23 dc       	srl \$3,\$tp
 39c:	2e 1c       	srl \$gp,\$1

0000039e <sll>:
 39e:	2b 0e       	sll \$11,\$0
 3a0:	2d 8e       	sll \$tp,\$8
 3a2:	28 9e       	sll \$8,\$9
 3a4:	2d fe       	sll \$tp,\$sp
 3a6:	2f fe       	sll \$sp,\$sp

000003a8 <srai>:
 3a8:	61 13       	sra \$1,0x2
 3aa:	6f 1b       	sra \$sp,0x3
 3ac:	6f 1b       	sra \$sp,0x3
 3ae:	66 23       	sra \$6,0x4
 3b0:	6f 1b       	sra \$sp,0x3

000003b2 <srli>:
 3b2:	6a 02       	srl \$10,0x0
 3b4:	69 1a       	srl \$9,0x3
 3b6:	66 22       	srl \$6,0x4
 3b8:	6a 12       	srl \$10,0x2
 3ba:	68 1a       	srl \$8,0x3

000003bc <slli>:
 3bc:	60 06       	sll \$0,0x0
 3be:	64 06       	sll \$4,0x0
 3c0:	6d 16       	sll \$tp,0x2
 3c2:	6b 16       	sll \$11,0x2
 3c4:	66 06       	sll \$6,0x0

000003c6 <sll3>:
 3c6:	6d 27       	sll3 \$0,\$tp,0x4
 3c8:	6e 07       	sll3 \$0,\$gp,0x0
 3ca:	68 17       	sll3 \$0,\$8,0x2
 3cc:	63 17       	sll3 \$0,\$3,0x2
 3ce:	68 07       	sll3 \$0,\$8,0x0

000003d0 <fsft>:
 3d0:	2e af       	fsft \$gp,\$10
 3d2:	2e 9f       	fsft \$gp,\$9
 3d4:	2f df       	fsft \$sp,\$tp
 3d6:	2b 3f       	fsft \$11,\$3
 3d8:	25 3f       	fsft \$5,\$3

000003da <bra>:
 3da:	b0 02       	bra 3dc <bra\+0x2>
 3dc:	bf fe       	bra 3da <bra>
 3de:	b0 02       	bra 3e0 <bra\+0x6>
 3e0:	b0 00       	bra 3e0 <bra\+0x6>
 3e2:	b0 02       	bra 3e4 <beqz>

000003e4 <beqz>:
 3e4:	a1 fe       	beqz \$1,3e2 <bra\+0x8>
 3e6:	af 02       	beqz \$sp,3e8 <beqz\+0x4>
 3e8:	a4 04       	beqz \$4,3ec <beqz\+0x8>
 3ea:	a4 00       	beqz \$4,3ea <beqz\+0x6>
 3ec:	a9 fe       	beqz \$9,3ea <beqz\+0x6>

000003ee <bnez>:
 3ee:	a8 03       	bnez \$8,3f0 <bnez\+0x2>
 3f0:	ad 03       	bnez \$tp,3f2 <bnez\+0x4>
 3f2:	ae 01       	bnez \$gp,3f2 <bnez\+0x4>
 3f4:	a6 03       	bnez \$6,3f6 <bnez\+0x8>
 3f6:	a8 fd       	bnez \$8,3f2 <bnez\+0x4>

000003f8 <beqi>:
 3f8:	ed 30 00 00 	beqi \$tp,0x3,3f8 <beqi>
 3fc:	e0 40 ff ff 	beqi \$0,0x4,3fa <beqi\+0x2>
 400:	ef 40 ff ff 	beqi \$sp,0x4,3fe <beqi\+0x6>
 404:	ed 20 00 00 	beqi \$tp,0x2,404 <beqi\+0xc>
 408:	e4 20 ff fc 	beqi \$4,0x2,400 <beqi\+0x8>

0000040c <bnei>:
 40c:	e8 14 00 00 	bnei \$8,0x1,40c <bnei>
 410:	e5 14 00 01 	bnei \$5,0x1,412 <bnei\+0x6>
 414:	e5 04 00 04 	bnei \$5,0x0,41c <bnei\+0x10>
 418:	e9 44 ff ff 	bnei \$9,0x4,416 <bnei\+0xa>
 41c:	e0 44 ff fc 	bnei \$0,0x4,414 <bnei\+0x8>

00000420 <blti>:
 420:	e7 3c 00 00 	blti \$7,0x3,420 <blti>
 424:	e1 1c 00 00 	blti \$1,0x1,424 <blti\+0x4>
 428:	e8 2c 00 01 	blti \$8,0x2,42a <blti\+0xa>
 42c:	eb 2c 00 01 	blti \$11,0x2,42e <blti\+0xe>
 430:	ef 3c ff ff 	blti \$sp,0x3,42e <blti\+0xe>

00000434 <bgei>:
 434:	e4 38 ff fc 	bgei \$4,0x3,42c <blti\+0xc>
 438:	e7 08 00 01 	bgei \$7,0x0,43a <bgei\+0x6>
 43c:	ed 18 00 00 	bgei \$tp,0x1,43c <bgei\+0x8>
 440:	e5 28 ff ff 	bgei \$5,0x2,43e <bgei\+0xa>
 444:	ec 48 ff fc 	bgei \$12,0x4,43c <bgei\+0x8>

00000448 <beq>:
 448:	e7 21 ff ff 	beq \$7,\$2,446 <bgei\+0x12>
 44c:	e1 31 ff fc 	beq \$1,\$3,444 <bgei\+0x10>
 450:	e2 01 00 01 	beq \$2,\$0,452 <beq\+0xa>
 454:	ef 81 00 01 	beq \$sp,\$8,456 <beq\+0xe>
 458:	e3 01 00 00 	beq \$3,\$0,458 <beq\+0x10>

0000045c <bne>:
 45c:	e6 35 00 00 	bne \$6,\$3,45c <bne>
 460:	ef 35 ff fc 	bne \$sp,\$3,458 <beq\+0x10>
 464:	e8 05 00 01 	bne \$8,\$0,466 <bne\+0xa>
 468:	ee f5 00 04 	bne \$gp,\$sp,470 <bsr12>
 46c:	ef 45 00 01 	bne \$sp,\$4,46e <bne\+0x12>

00000470 <bsr12>:
 470:	b0 03       	bsr 472 <bsr12\+0x2>
 472:	bf f9       	bsr 46a <bne\+0xe>
 474:	bf f1       	bsr 464 <bne\+0x8>
 476:	bf ff       	bsr 474 <bsr12\+0x4>
 478:	bf f9       	bsr 470 <bsr12>

0000047a <bsr24>:
 47a:	b0 05       	bsr 47e <bsr24\+0x4>
 47c:	bf ff       	bsr 47a <bsr24>
 47e:	bf fd       	bsr 47a <bsr24>
 480:	b0 01       	bsr 480 <bsr24\+0x6>
 482:	b0 03       	bsr 484 <jmp>

00000484 <jmp>:
 484:	10 2e       	jmp \$2
 486:	10 de       	jmp \$tp
 488:	10 5e       	jmp \$5
 48a:	10 fe       	jmp \$sp
 48c:	10 8e       	jmp \$8

0000048e <jmp24>:
 48e:	d8 28 00 00 	jmp 4 <sb\+0x4>
 492:	d8 18 00 00 	jmp 2 <sb\+0x2>
 496:	d8 08 00 00 	jmp 0 <sb>
 49a:	d8 18 00 00 	jmp 2 <sb\+0x2>
 49e:	d8 28 00 00 	jmp 4 <sb\+0x4>

000004a2 <jsr>:
 4a2:	10 ff       	jsr \$sp
 4a4:	10 df       	jsr \$tp
 4a6:	10 df       	jsr \$tp
 4a8:	10 6f       	jsr \$6
 4aa:	10 6f       	jsr \$6

000004ac <ret>:
 4ac:	70 02       	ret

000004ae <repeat>:
 4ae:	e4 09 00 01 	repeat \$4,4b0 <repeat\+0x2>
 4b2:	e8 09 00 02 	repeat \$8,4b6 <repeat\+0x8>
 4b6:	e0 09 00 04 	repeat \$0,4be <repeat\+0x10>
 4ba:	e6 09 00 01 	repeat \$6,4bc <repeat\+0xe>
 4be:	e4 09 00 01 	repeat \$4,4c0 <repeat\+0x12>

000004c2 <erepeat>:
 4c2:	e0 19 00 01 	erepeat 4c4 <erepeat\+0x2>
 4c6:	e0 19 00 00 	erepeat 4c6 <erepeat\+0x4>
 4ca:	e0 19 00 01 	erepeat 4cc <erepeat\+0xa>
 4ce:	e0 19 ff ff 	erepeat 4cc <erepeat\+0xa>
 4d2:	e0 19 00 00 	erepeat 4d2 <erepeat\+0x10>

000004d6 <stc>:
 4d6:	7d e8       	stc \$tp,\$mb1
 4d8:	7d c9       	stc \$tp,\$ccfg
 4da:	7b 89       	stc \$11,\$dbg
 4dc:	7a c9       	stc \$10,\$ccfg
 4de:	79 39       	stc \$9,\$epc

000004e0 <ldc>:
 4e0:	7d 8a       	ldc \$tp,\$lo
 4e2:	78 7b       	ldc \$8,\$npc
 4e4:	79 ca       	ldc \$9,\$mb0
 4e6:	7f 2a       	ldc \$sp,\$sar
 4e8:	79 cb       	ldc \$9,\$ccfg

000004ea <di>:
 4ea:	70 00       	di

000004ec <ei>:
 4ec:	70 10       	ei

000004ee <reti>:
 4ee:	70 12       	reti

000004f0 <halt>:
 4f0:	70 22       	halt

000004f2 <swi>:
 4f2:	70 26       	swi 0x2
 4f4:	70 06       	swi 0x0
 4f6:	70 26       	swi 0x2
 4f8:	70 36       	swi 0x3
 4fa:	70 16       	swi 0x1

000004fc <break>:
 4fc:	70 32       	break

000004fe <syncm>:
 4fe:	70 11       	syncm

00000500 <stcb>:
 500:	f5 04 00 04 	stcb \$5,0x4
 504:	f5 04 00 01 	stcb \$5,0x1
 508:	fe 04 00 00 	stcb \$gp,0x0
 50c:	ff 04 00 04 	stcb \$sp,0x4
 510:	fb 04 00 02 	stcb \$11,0x2

00000514 <ldcb>:
 514:	f2 14 00 03 	ldcb \$2,0x3
 518:	f2 14 00 04 	ldcb \$2,0x4
 51c:	f9 14 00 01 	ldcb \$9,0x1
 520:	fa 14 00 04 	ldcb \$10,0x4
 524:	f1 14 00 04 	ldcb \$1,0x4

00000528 <bsetm>:
 528:	20 a0       	bsetm \(\$10\),0x0
 52a:	20 f0       	bsetm \(\$sp\),0x0
 52c:	22 10       	bsetm \(\$1\),0x2
 52e:	24 f0       	bsetm \(\$sp\),0x4
 530:	24 80       	bsetm \(\$8\),0x4

00000532 <bclrm>:
 532:	20 51       	bclrm \(\$5\),0x0
 534:	22 51       	bclrm \(\$5\),0x2
 536:	20 81       	bclrm \(\$8\),0x0
 538:	22 91       	bclrm \(\$9\),0x2
 53a:	23 51       	bclrm \(\$5\),0x3

0000053c <bnotm>:
 53c:	24 e2       	bnotm \(\$gp\),0x4
 53e:	24 b2       	bnotm \(\$11\),0x4
 540:	20 a2       	bnotm \(\$10\),0x0
 542:	24 d2       	bnotm \(\$tp\),0x4
 544:	20 82       	bnotm \(\$8\),0x0

00000546 <btstm>:
 546:	20 e3       	btstm \$0,\(\$gp\),0x0
 548:	21 e3       	btstm \$0,\(\$gp\),0x1
 54a:	20 b3       	btstm \$0,\(\$11\),0x0
 54c:	23 e3       	btstm \$0,\(\$gp\),0x3
 54e:	22 83       	btstm \$0,\(\$8\),0x2

00000550 <tas>:
 550:	27 d4       	tas \$7,\(\$tp\)
 552:	27 c4       	tas \$7,\(\$12\)
 554:	23 84       	tas \$3,\(\$8\)
 556:	22 54       	tas \$2,\(\$5\)
 558:	26 a4       	tas \$6,\(\$10\)

0000055a <cache>:
 55a:	71 d4       	cache 0x1,\(\$tp\)
 55c:	73 c4       	cache 0x3,\(\$12\)
 55e:	73 94       	cache 0x3,\(\$9\)
 560:	74 24       	cache 0x4,\(\$2\)
 562:	74 74       	cache 0x4,\(\$7\)

00000564 <mul>:
 564:	18 e4       	mul \$8,\$gp
 566:	12 94       	mul \$2,\$9
 568:	1e f4       	mul \$gp,\$sp
 56a:	19 74       	mul \$9,\$7
 56c:	17 b4       	mul \$7,\$11

0000056e <mulu>:
 56e:	12 55       	mulu \$2,\$5
 570:	16 e5       	mulu \$6,\$gp
 572:	1e f5       	mulu \$gp,\$sp
 574:	1b e5       	mulu \$11,\$gp
 576:	13 95       	mulu \$3,\$9

00000578 <mulr>:
 578:	1c 66       	mulr \$12,\$6
 57a:	1d 86       	mulr \$tp,\$8
 57c:	17 a6       	mulr \$7,\$10
 57e:	1e 16       	mulr \$gp,\$1
 580:	10 f6       	mulr \$0,\$sp

00000582 <mulru>:
 582:	14 27       	mulru \$4,\$2
 584:	1e 17       	mulru \$gp,\$1
 586:	1f 47       	mulru \$sp,\$4
 588:	1a 67       	mulru \$10,\$6
 58a:	10 e7       	mulru \$0,\$gp

0000058c <madd>:
 58c:	f4 b1 30 04 	madd \$4,\$11
 590:	ff e1 30 04 	madd \$sp,\$gp
 594:	fe f1 30 04 	madd \$gp,\$sp
 598:	f4 d1 30 04 	madd \$4,\$tp
 59c:	f1 e1 30 04 	madd \$1,\$gp

000005a0 <maddu>:
 5a0:	f0 11 30 05 	maddu \$0,\$1
 5a4:	f7 61 30 05 	maddu \$7,\$6
 5a8:	f9 51 30 05 	maddu \$9,\$5
 5ac:	fe f1 30 05 	maddu \$gp,\$sp
 5b0:	f7 d1 30 05 	maddu \$7,\$tp

000005b4 <maddr>:
 5b4:	f6 81 30 06 	maddr \$6,\$8
 5b8:	f9 e1 30 06 	maddr \$9,\$gp
 5bc:	f8 e1 30 06 	maddr \$8,\$gp
 5c0:	f3 21 30 06 	maddr \$3,\$2
 5c4:	f1 b1 30 06 	maddr \$1,\$11

000005c8 <maddru>:
 5c8:	fa 31 30 07 	maddru \$10,\$3
 5cc:	ff c1 30 07 	maddru \$sp,\$12
 5d0:	f8 81 30 07 	maddru \$8,\$8
 5d4:	fe 31 30 07 	maddru \$gp,\$3
 5d8:	f8 f1 30 07 	maddru \$8,\$sp

000005dc <div>:
 5dc:	19 38       	div \$9,\$3
 5de:	14 e8       	div \$4,\$gp
 5e0:	12 c8       	div \$2,\$12
 5e2:	18 d8       	div \$8,\$tp
 5e4:	1d 68       	div \$tp,\$6

000005e6 <divu>:
 5e6:	19 59       	divu \$9,\$5
 5e8:	18 d9       	divu \$8,\$tp
 5ea:	10 e9       	divu \$0,\$gp
 5ec:	19 59       	divu \$9,\$5
 5ee:	10 59       	divu \$0,\$5

000005f0 <dret>:
 5f0:	70 13       	dret

000005f2 <dbreak>:
 5f2:	70 33       	dbreak

000005f4 <ldz>:
 5f4:	fe 41 00 00 	ldz \$gp,\$4
 5f8:	fa b1 00 00 	ldz \$10,\$11
 5fc:	f9 91 00 00 	ldz \$9,\$9
 600:	ff d1 00 00 	ldz \$sp,\$tp
 604:	fe 31 00 00 	ldz \$gp,\$3

00000608 <abs>:
 608:	ff 91 00 03 	abs \$sp,\$9
 60c:	f5 41 00 03 	abs \$5,\$4
 610:	fd d1 00 03 	abs \$tp,\$tp
 614:	f0 31 00 03 	abs \$0,\$3
 618:	f3 e1 00 03 	abs \$3,\$gp

0000061c <ave>:
 61c:	fb a1 00 02 	ave \$11,\$10
 620:	f8 a1 00 02 	ave \$8,\$10
 624:	fe 21 00 02 	ave \$gp,\$2
 628:	fa c1 00 02 	ave \$10,\$12
 62c:	ff 81 00 02 	ave \$sp,\$8

00000630 <min>:
 630:	f8 31 00 04 	min \$8,\$3
 634:	f7 01 00 04 	min \$7,\$0
 638:	f2 21 00 04 	min \$2,\$2
 63c:	f5 61 00 04 	min \$5,\$6
 640:	fb 51 00 04 	min \$11,\$5

00000644 <max>:
 644:	fb f1 00 05 	max \$11,\$sp
 648:	fe 01 00 05 	max \$gp,\$0
 64c:	fc f1 00 05 	max \$12,\$sp
 650:	fe 21 00 05 	max \$gp,\$2
 654:	fe f1 00 05 	max \$gp,\$sp

00000658 <minu>:
 658:	fb 81 00 06 	minu \$11,\$8
 65c:	f7 51 00 06 	minu \$7,\$5
 660:	f8 e1 00 06 	minu \$8,\$gp
 664:	fb 41 00 06 	minu \$11,\$4
 668:	f2 f1 00 06 	minu \$2,\$sp

0000066c <maxu>:
 66c:	f3 31 00 07 	maxu \$3,\$3
 670:	fd 01 00 07 	maxu \$tp,\$0
 674:	f4 81 00 07 	maxu \$4,\$8
 678:	fe 21 00 07 	maxu \$gp,\$2
 67c:	fc 81 00 07 	maxu \$12,\$8

00000680 <clip>:
 680:	fa 01 10 08 	clip \$10,0x1
 684:	ff 01 10 20 	clip \$sp,0x4
 688:	f4 01 10 18 	clip \$4,0x3
 68c:	ff 01 10 18 	clip \$sp,0x3
 690:	f1 01 10 00 	clip \$1,0x0

00000694 <clipu>:
 694:	fa 01 10 21 	clipu \$10,0x4
 698:	fd 01 10 09 	clipu \$tp,0x1
 69c:	f5 01 10 21 	clipu \$5,0x4
 6a0:	fe 01 10 01 	clipu \$gp,0x0
 6a4:	f5 01 10 09 	clipu \$5,0x1

000006a8 <sadd>:
 6a8:	f5 01 00 08 	sadd \$5,\$0
 6ac:	ff 31 00 08 	sadd \$sp,\$3
 6b0:	f0 a1 00 08 	sadd \$0,\$10
 6b4:	ff c1 00 08 	sadd \$sp,\$12
 6b8:	f4 21 00 08 	sadd \$4,\$2

000006bc <ssub>:
 6bc:	f1 a1 00 0a 	ssub \$1,\$10
 6c0:	f4 71 00 0a 	ssub \$4,\$7
 6c4:	f8 31 00 0a 	ssub \$8,\$3
 6c8:	f7 e1 00 0a 	ssub \$7,\$gp
 6cc:	fd 41 00 0a 	ssub \$tp,\$4

000006d0 <saddu>:
 6d0:	f9 e1 00 09 	saddu \$9,\$gp
 6d4:	f0 a1 00 09 	saddu \$0,\$10
 6d8:	f7 c1 00 09 	saddu \$7,\$12
 6dc:	f5 f1 00 09 	saddu \$5,\$sp
 6e0:	fd 31 00 09 	saddu \$tp,\$3

000006e4 <ssubu>:
 6e4:	ff e1 00 0b 	ssubu \$sp,\$gp
 6e8:	f0 f1 00 0b 	ssubu \$0,\$sp
 6ec:	f3 a1 00 0b 	ssubu \$3,\$10
 6f0:	ff d1 00 0b 	ssubu \$sp,\$tp
 6f4:	f2 91 00 0b 	ssubu \$2,\$9

000006f8 <swcp>:
 6f8:	33 d8       	swcp \$c3,\(\$tp\)
 6fa:	3f d8       	swcp \$c15,\(\$tp\)
 6fc:	3d 08       	swcp \$c13,\(\$0\)
 6fe:	3c c8       	swcp \$c12,\(\$12\)
 700:	39 e8       	swcp \$c9,\(\$gp\)

00000702 <lwcp>:
 702:	37 39       	lwcp \$c7,\(\$3\)
 704:	36 39       	lwcp \$c6,\(\$3\)
 706:	30 29       	lwcp \$c0,\(\$2\)
 708:	38 89       	lwcp \$c8,\(\$8\)
 70a:	3b d9       	lwcp \$c11,\(\$tp\)

0000070c <smcp>:
 70c:	3e 9a       	smcp \$c14,\(\$9\)
 70e:	32 8a       	smcp \$c2,\(\$8\)
 710:	3e fa       	smcp \$c14,\(\$sp\)
 712:	3a 8a       	smcp \$c10,\(\$8\)
 714:	32 8a       	smcp \$c2,\(\$8\)

00000716 <lmcp>:
 716:	3b 1b       	lmcp \$c11,\(\$1\)
 718:	38 8b       	lmcp \$c8,\(\$8\)
 71a:	3b db       	lmcp \$c11,\(\$tp\)
 71c:	38 0b       	lmcp \$c8,\(\$0\)
 71e:	38 eb       	lmcp \$c8,\(\$gp\)

00000720 <swcpi>:
 720:	37 00       	swcpi \$c7,\(\$0\+\)
 722:	36 e0       	swcpi \$c6,\(\$gp\+\)
 724:	3c 80       	swcpi \$c12,\(\$8\+\)
 726:	3e f0       	swcpi \$c14,\(\$sp\+\)
 728:	36 00       	swcpi \$c6,\(\$0\+\)

0000072a <lwcpi>:
 72a:	38 21       	lwcpi \$c8,\(\$2\+\)
 72c:	39 01       	lwcpi \$c9,\(\$0\+\)
 72e:	33 e1       	lwcpi \$c3,\(\$gp\+\)
 730:	3d 51       	lwcpi \$c13,\(\$5\+\)
 732:	3b e1       	lwcpi \$c11,\(\$gp\+\)

00000734 <smcpi>:
 734:	38 22       	smcpi \$c8,\(\$2\+\)
 736:	3b 92       	smcpi \$c11,\(\$9\+\)
 738:	34 32       	smcpi \$c4,\(\$3\+\)
 73a:	3e 22       	smcpi \$c14,\(\$2\+\)
 73c:	39 32       	smcpi \$c9,\(\$3\+\)

0000073e <lmcpi>:
 73e:	36 e3       	lmcpi \$c6,\(\$gp\+\)
 740:	39 53       	lmcpi \$c9,\(\$5\+\)
 742:	3a 63       	lmcpi \$c10,\(\$6\+\)
 744:	31 63       	lmcpi \$c1,\(\$6\+\)
 746:	32 83       	lmcpi \$c2,\(\$8\+\)

00000748 <swcp16>:
 748:	f0 2c ff ff 	swcp \$c0,-1\(\$2\)
 74c:	f5 ac 00 01 	swcp \$c5,1\(\$10\)
 750:	f8 cc 00 02 	swcp \$c8,2\(\$12\)
 754:	fe 1c ff ff 	swcp \$c14,-1\(\$1\)
 758:	fc 3c 00 02 	swcp \$c12,2\(\$3\)

0000075c <lwcp16>:
 75c:	f8 5d ff ff 	lwcp \$c8,-1\(\$5\)
 760:	fc fd 00 01 	lwcp \$c12,1\(\$sp\)
 764:	f1 0d 00 02 	lwcp \$c1,2\(\$0\)
 768:	f4 dd 00 01 	lwcp \$c4,1\(\$tp\)
 76c:	f6 bd 00 02 	lwcp \$c6,2\(\$11\)

00000770 <smcp16>:
 770:	f9 ae ff ff 	smcp \$c9,-1\(\$10\)
 774:	fe ee 00 01 	smcp \$c14,1\(\$gp\)
 778:	f3 fe 00 02 	smcp \$c3,2\(\$sp\)
 77c:	ff 8e ff fe 	smcp \$c15,-2\(\$8\)
 780:	fd de 00 01 	smcp \$c13,1\(\$tp\)

00000784 <lmcp16>:
 784:	f0 ff 00 01 	lmcp \$c0,1\(\$sp\)
 788:	ff 8f 00 01 	lmcp \$c15,1\(\$8\)
 78c:	f2 8f ff ff 	lmcp \$c2,-1\(\$8\)
 790:	fe 8f 00 01 	lmcp \$c14,1\(\$8\)
 794:	f1 af ff ff 	lmcp \$c1,-1\(\$10\)

00000798 <sbcpa>:
 798:	fe f5 00 02 	sbcpa \$c14,\(\$sp\+\),2
 79c:	f2 45 00 fe 	sbcpa \$c2,\(\$4\+\),-2
 7a0:	f8 15 00 00 	sbcpa \$c8,\(\$1\+\),0
 7a4:	fb 35 00 00 	sbcpa \$c11,\(\$3\+\),0
 7a8:	f9 e5 00 fe 	sbcpa \$c9,\(\$gp\+\),-2

000007ac <lbcpa>:
 7ac:	f7 25 40 fe 	lbcpa \$c7,\(\$2\+\),-2
 7b0:	fc f5 40 02 	lbcpa \$c12,\(\$sp\+\),2
 7b4:	f5 45 40 fe 	lbcpa \$c5,\(\$4\+\),-2
 7b8:	f7 45 40 fe 	lbcpa \$c7,\(\$4\+\),-2
 7bc:	f8 f5 40 00 	lbcpa \$c8,\(\$sp\+\),0

000007c0 <shcpa>:
 7c0:	f0 e5 10 00 	shcpa \$c0,\(\$gp\+\),0
 7c4:	fc f5 10 10 	shcpa \$c12,\(\$sp\+\),16
 7c8:	f1 45 10 04 	shcpa \$c1,\(\$4\+\),4
 7cc:	f5 45 10 e0 	shcpa \$c5,\(\$4\+\),-32
 7d0:	f1 f5 10 00 	shcpa \$c1,\(\$sp\+\),0

000007d4 <lhcpa>:
 7d4:	f4 45 50 00 	lhcpa \$c4,\(\$4\+\),0
 7d8:	f6 55 50 30 	lhcpa \$c6,\(\$5\+\),48
 7dc:	f3 65 50 cc 	lhcpa \$c3,\(\$6\+\),-52
 7e0:	f8 65 50 e8 	lhcpa \$c8,\(\$6\+\),-24
 7e4:	f0 95 50 00 	lhcpa \$c0,\(\$9\+\),0

000007e8 <swcpa>:
 7e8:	f1 95 20 10 	swcpa \$c1,\(\$9\+\),16
 7ec:	f7 f5 20 20 	swcpa \$c7,\(\$sp\+\),32
 7f0:	f3 c5 20 30 	swcpa \$c3,\(\$12\+\),48
 7f4:	fa 95 20 08 	swcpa \$c10,\(\$9\+\),8
 7f8:	fe 85 20 04 	swcpa \$c14,\(\$8\+\),4

000007fc <lwcpa>:
 7fc:	f6 e5 60 f8 	lwcpa \$c6,\(\$gp\+\),-8
 800:	f4 75 60 04 	lwcpa \$c4,\(\$7\+\),4
 804:	fb e5 60 f0 	lwcpa \$c11,\(\$gp\+\),-16
 808:	fa f5 60 e0 	lwcpa \$c10,\(\$sp\+\),-32
 80c:	f2 25 60 08 	lwcpa \$c2,\(\$2\+\),8

00000810 <smcpa>:
 810:	fd f5 30 f8 	smcpa \$c13,\(\$sp\+\),-8
 814:	f6 75 30 f8 	smcpa \$c6,\(\$7\+\),-8
 818:	f5 35 30 10 	smcpa \$c5,\(\$3\+\),16
 81c:	fd f5 30 10 	smcpa \$c13,\(\$sp\+\),16
 820:	f3 c5 30 30 	smcpa \$c3,\(\$12\+\),48

00000824 <lmcpa>:
 824:	f9 45 70 00 	lmcpa \$c9,\(\$4\+\),0
 828:	f3 f5 70 f0 	lmcpa \$c3,\(\$sp\+\),-16
 82c:	ff d5 70 08 	lmcpa \$c15,\(\$tp\+\),8
 830:	f8 85 70 f8 	lmcpa \$c8,\(\$8\+\),-8
 834:	fa 95 70 00 	lmcpa \$c10,\(\$9\+\),0

00000838 <sbcpm0>:
 838:	fa d5 08 08 	sbcpm0 \$c10,\(\$tp\+\),8
 83c:	fd 55 08 f8 	sbcpm0 \$c13,\(\$5\+\),-8
 840:	f4 55 08 f8 	sbcpm0 \$c4,\(\$5\+\),-8
 844:	fa d5 08 10 	sbcpm0 \$c10,\(\$tp\+\),16
 848:	f4 55 08 e8 	sbcpm0 \$c4,\(\$5\+\),-24

0000084c <lbcpm0>:
 84c:	f0 45 48 00 	lbcpm0 \$c0,\(\$4\+\),0
 850:	f9 75 48 f8 	lbcpm0 \$c9,\(\$7\+\),-8
 854:	fc 85 48 18 	lbcpm0 \$c12,\(\$8\+\),24
 858:	f8 c5 48 10 	lbcpm0 \$c8,\(\$12\+\),16
 85c:	f7 85 48 10 	lbcpm0 \$c7,\(\$8\+\),16

00000860 <shcpm0>:
 860:	f2 d5 18 02 	shcpm0 \$c2,\(\$tp\+\),2
 864:	f7 f5 18 fe 	shcpm0 \$c7,\(\$sp\+\),-2
 868:	f8 25 18 02 	shcpm0 \$c8,\(\$2\+\),2
 86c:	fd 55 18 00 	shcpm0 \$c13,\(\$5\+\),0
 870:	f3 e5 18 08 	shcpm0 \$c3,\(\$gp\+\),8

00000874 <lhcpm0>:
 874:	f7 45 58 08 	lhcpm0 \$c7,\(\$4\+\),8
 878:	f3 35 58 fe 	lhcpm0 \$c3,\(\$3\+\),-2
 87c:	f3 15 58 00 	lhcpm0 \$c3,\(\$1\+\),0
 880:	f2 e5 58 00 	lhcpm0 \$c2,\(\$gp\+\),0
 884:	fc 65 58 02 	lhcpm0 \$c12,\(\$6\+\),2

00000888 <swcpm0>:
 888:	f8 85 28 20 	swcpm0 \$c8,\(\$8\+\),32
 88c:	f9 f5 28 00 	swcpm0 \$c9,\(\$sp\+\),0
 890:	f9 25 28 f0 	swcpm0 \$c9,\(\$2\+\),-16
 894:	f0 e5 28 30 	swcpm0 \$c0,\(\$gp\+\),48
 898:	ff 15 28 08 	swcpm0 \$c15,\(\$1\+\),8

0000089c <lwcpm0>:
 89c:	fe a5 68 fc 	lwcpm0 \$c14,\(\$10\+\),-4
 8a0:	fb f5 68 fc 	lwcpm0 \$c11,\(\$sp\+\),-4
 8a4:	f5 75 68 f8 	lwcpm0 \$c5,\(\$7\+\),-8
 8a8:	f2 c5 68 20 	lwcpm0 \$c2,\(\$12\+\),32
 8ac:	f2 e5 68 10 	lwcpm0 \$c2,\(\$gp\+\),16

000008b0 <smcpm0>:
 8b0:	f1 c5 38 08 	smcpm0 \$c1,\(\$12\+\),8
 8b4:	f8 45 38 f0 	smcpm0 \$c8,\(\$4\+\),-16
 8b8:	fa b5 38 00 	smcpm0 \$c10,\(\$11\+\),0
 8bc:	f1 35 38 f0 	smcpm0 \$c1,\(\$3\+\),-16
 8c0:	fb f5 38 f8 	smcpm0 \$c11,\(\$sp\+\),-8

000008c4 <lmcpm0>:
 8c4:	fe a5 78 00 	lmcpm0 \$c14,\(\$10\+\),0
 8c8:	f6 f5 78 f0 	lmcpm0 \$c6,\(\$sp\+\),-16
 8cc:	fd 15 78 08 	lmcpm0 \$c13,\(\$1\+\),8
 8d0:	fa d5 78 e8 	lmcpm0 \$c10,\(\$tp\+\),-24
 8d4:	f7 e5 78 e8 	lmcpm0 \$c7,\(\$gp\+\),-24

000008d8 <sbcpm1>:
 8d8:	f9 85 0c 00 	sbcpm1 \$c9,\(\$8\+\),0
 8dc:	f7 c5 0c e8 	sbcpm1 \$c7,\(\$12\+\),-24
 8e0:	ff 55 0c e8 	sbcpm1 \$c15,\(\$5\+\),-24
 8e4:	f5 d5 0c 10 	sbcpm1 \$c5,\(\$tp\+\),16
 8e8:	f6 15 0c 80 	sbcpm1 \$c6,\(\$1\+\),-128

000008ec <lbcpm1>:
 8ec:	f6 e5 4c 02 	lbcpm1 \$c6,\(\$gp\+\),2
 8f0:	f7 d5 4c fe 	lbcpm1 \$c7,\(\$tp\+\),-2
 8f4:	f4 d5 4c 01 	lbcpm1 \$c4,\(\$tp\+\),1
 8f8:	fc 25 4c fe 	lbcpm1 \$c12,\(\$2\+\),-2
 8fc:	fb 75 4c 01 	lbcpm1 \$c11,\(\$7\+\),1

00000900 <shcpm1>:
 900:	f4 85 1c 18 	shcpm1 \$c4,\(\$8\+\),24
 904:	fb 65 1c f0 	shcpm1 \$c11,\(\$6\+\),-16
 908:	f7 85 1c 08 	shcpm1 \$c7,\(\$8\+\),8
 90c:	f5 c5 1c 10 	shcpm1 \$c5,\(\$12\+\),16
 910:	f0 85 1c e0 	shcpm1 \$c0,\(\$8\+\),-32

00000914 <lhcpm1>:
 914:	fb 05 5c 00 	lhcpm1 \$c11,\(\$0\+\),0
 918:	f7 d5 5c fe 	lhcpm1 \$c7,\(\$tp\+\),-2
 91c:	fa 85 5c 08 	lhcpm1 \$c10,\(\$8\+\),8
 920:	f3 d5 5c 00 	lhcpm1 \$c3,\(\$tp\+\),0
 924:	f9 65 5c 02 	lhcpm1 \$c9,\(\$6\+\),2

00000928 <swcpm1>:
 928:	f9 85 2c 18 	swcpm1 \$c9,\(\$8\+\),24
 92c:	f9 e5 2c 00 	swcpm1 \$c9,\(\$gp\+\),0
 930:	f9 85 2c 10 	swcpm1 \$c9,\(\$8\+\),16
 934:	fe 15 2c 00 	swcpm1 \$c14,\(\$1\+\),0
 938:	f2 f5 2c 08 	swcpm1 \$c2,\(\$sp\+\),8

0000093c <lwcpm1>:
 93c:	f8 85 6c 00 	lwcpm1 \$c8,\(\$8\+\),0
 940:	f3 e5 6c f0 	lwcpm1 \$c3,\(\$gp\+\),-16
 944:	f7 65 6c f8 	lwcpm1 \$c7,\(\$6\+\),-8
 948:	fe 85 6c e8 	lwcpm1 \$c14,\(\$8\+\),-24
 94c:	f3 85 6c 18 	lwcpm1 \$c3,\(\$8\+\),24

00000950 <smcpm1>:
 950:	fa 45 3c 00 	smcpm1 \$c10,\(\$4\+\),0
 954:	f6 f5 3c f0 	smcpm1 \$c6,\(\$sp\+\),-16
 958:	fd 75 3c e8 	smcpm1 \$c13,\(\$7\+\),-24
 95c:	f3 e5 3c f8 	smcpm1 \$c3,\(\$gp\+\),-8
 960:	f0 25 3c 08 	smcpm1 \$c0,\(\$2\+\),8

00000964 <lmcpm1>:
 964:	fc 15 7c 00 	lmcpm1 \$c12,\(\$1\+\),0
 968:	f0 65 7c 08 	lmcpm1 \$c0,\(\$6\+\),8
 96c:	f6 25 7c f8 	lmcpm1 \$c6,\(\$2\+\),-8
 970:	fc e5 7c f0 	lmcpm1 \$c12,\(\$gp\+\),-16
 974:	fe f5 7c 30 	lmcpm1 \$c14,\(\$sp\+\),48

00000... <bcpeq>:
 ...:	d8 44 00 00 	bcpeq 0x4,... <bcpeq>
 ...:	d8 04 ff ff 	bcpeq 0x0,... <bcpeq\+0x2>
 ...:	d8 44 ff ff 	bcpeq 0x4,... <bcpeq\+0x6>
 ...:	d8 14 00 01 	bcpeq 0x1,... <bcpeq\+0xe>
 ...:	d8 24 00 01 	bcpeq 0x2,... <bcpeq\+0x12>

00000... <bcpne>:
 ...:	d8 25 00 00 	bcpne 0x2,... <bcpne>
 ...:	d8 45 00 00 	bcpne 0x4,... <bcpne\+0x4>
 ...:	d8 15 00 00 	bcpne 0x1,... <bcpne\+0x8>
 ...:	d8 45 00 00 	bcpne 0x4,... <bcpne\+0xc>
 ...:	d8 15 00 01 	bcpne 0x1,... <bcpne\+0x12>

00000... <bcpat>:
 ...:	d8 16 ff ff 	bcpat 0x1,... <bcpne\+0x12>
 ...:	d8 06 00 01 	bcpat 0x0,... <bcpat\+0x6>
 ...:	d8 06 ff ff 	bcpat 0x0,... <bcpat\+0x6>
 ...:	d8 26 00 00 	bcpat 0x2,... <bcpat\+0xc>
 ...:	d8 16 ff ff 	bcpat 0x1,... <bcpat\+0xe>

00000... <bcpaf>:
 ...:	d8 47 00 00 	bcpaf 0x4,... <bcpaf>
 ...:	d8 37 00 00 	bcpaf 0x3,... <bcpaf\+0x4>
 ...:	d8 47 00 00 	bcpaf 0x4,... <bcpaf\+0x8>
 ...:	d8 17 00 01 	bcpaf 0x1,... <bcpaf\+0xe>
 ...:	d8 47 00 01 	bcpaf 0x4,... <bcpaf\+0x12>

00000... <synccp>:
 ...:	70 21       	synccp

00000... <jsrv>:
 ...:	18 bf       	jsrv \$11
 ...:	18 5f       	jsrv \$5
 ...:	18 af       	jsrv \$10
 ...:	18 cf       	jsrv \$12
 ...:	18 af       	jsrv \$10

00000... <bsrv>:
 ...:	df fb ff ff 	bsrv ... <jsrv\+0x8>
 ...:	df fb ff ff 	bsrv ... <bsrv\+0x2>
 ...:	df fb ff ff 	bsrv ... <bsrv\+0x6>
 ...:	d8 1b 00 00 	bsrv ... <bsrv\+0xe>
 ...:	d8 0b 00 00 	bsrv ... <bsrv\+0x10>

00000... <case106341>:
 ...:	7a 78       	stc \$10,\$hi
 ...:	70 8a       	ldc \$0,\$lo

00000... <case106821>:
 ...:	00 08       	sb \$0,\(\$0\)
 ...:	00 09       	sh \$0,\(\$0\)
 ...:	00 0a       	sw \$0,\(\$0\)
 ...:	00 0c       	lb \$0,\(\$0\)
 ...:	00 0d       	lh \$0,\(\$0\)
 ...:	00 0e       	lw \$0,\(\$0\)
 ...:	00 0b       	lbu \$0,\(\$0\)
 ...:	00 0f       	lhu \$0,\(\$0\)
 ...:	00 08       	sb \$0,\(\$0\)
 ...:	00 08       	sb \$0,\(\$0\)
 ...:	00 08       	sb \$0,\(\$0\)
 ...:	00 08       	sb \$0,\(\$0\)
 ...:	00 08       	sb \$0,\(\$0\)
 ...:	00 08       	sb \$0,\(\$0\)
 ...:	00 09       	sh \$0,\(\$0\)
 ...:	00 09       	sh \$0,\(\$0\)
 ...:	00 09       	sh \$0,\(\$0\)
 ...:	00 09       	sh \$0,\(\$0\)
 ...:	00 09       	sh \$0,\(\$0\)
 ...:	00 09       	sh \$0,\(\$0\)
 ...:	00 0a       	sw \$0,\(\$0\)
 ...:	00 0a       	sw \$0,\(\$0\)
 ...:	00 0a       	sw \$0,\(\$0\)
 ...:	00 0a       	sw \$0,\(\$0\)
 ...:	00 0a       	sw \$0,\(\$0\)
 ...:	00 0a       	sw \$0,\(\$0\)
 ...:	00 0c       	lb \$0,\(\$0\)
 ...:	00 0c       	lb \$0,\(\$0\)
 ...:	00 0c       	lb \$0,\(\$0\)
 ...:	00 0c       	lb \$0,\(\$0\)
 ...:	00 0c       	lb \$0,\(\$0\)
 ...:	00 0c       	lb \$0,\(\$0\)
 ...:	00 0d       	lh \$0,\(\$0\)
 ...:	00 0d       	lh \$0,\(\$0\)
 ...:	00 0d       	lh \$0,\(\$0\)
 ...:	00 0d       	lh \$0,\(\$0\)
 ...:	00 0d       	lh \$0,\(\$0\)
 ...:	00 0d       	lh \$0,\(\$0\)
 ...:	00 0e       	lw \$0,\(\$0\)
 ...:	00 0e       	lw \$0,\(\$0\)
 ...:	00 0e       	lw \$0,\(\$0\)
 ...:	00 0e       	lw \$0,\(\$0\)
 ...:	00 0e       	lw \$0,\(\$0\)
 ...:	00 0e       	lw \$0,\(\$0\)
 ...:	00 0b       	lbu \$0,\(\$0\)
 ...:	00 0b       	lbu \$0,\(\$0\)
 ...:	00 0b       	lbu \$0,\(\$0\)
 ...:	00 0b       	lbu \$0,\(\$0\)
 ...:	00 0b       	lbu \$0,\(\$0\)
 ...:	00 0b       	lbu \$0,\(\$0\)
 ...:	00 0f       	lhu \$0,\(\$0\)
 ...:	00 0f       	lhu \$0,\(\$0\)
 ...:	00 0f       	lhu \$0,\(\$0\)
 ...:	00 0f       	lhu \$0,\(\$0\)
 ...:	00 0f       	lhu \$0,\(\$0\)
 ...:	00 0f       	lhu \$0,\(\$0\)
 ...:	c0 08 00 01 	sb \$0,1\(\$0\)
 ...:	c0 08 00 01 	sb \$0,1\(\$0\)
 ...:	c0 08 00 00 	sb \$0,0\(\$0\)
 ...:	c0 08 00 00 	sb \$0,0\(\$0\)
 ...:	c0 08 00 01 	sb \$0,1\(\$0\)
 ...:	c0 08 00 01 	sb \$0,1\(\$0\)
 ...:	c0 09 00 01 	sh \$0,1\(\$0\)
 ...:	c0 09 00 01 	sh \$0,1\(\$0\)
 ...:	c0 09 00 00 	sh \$0,0\(\$0\)
 ...:	c0 09 00 00 	sh \$0,0\(\$0\)
 ...:	c0 09 00 01 	sh \$0,1\(\$0\)
 ...:	c0 09 00 01 	sh \$0,1\(\$0\)
 ...:	c0 0a 00 01 	sw \$0,1\(\$0\)
 ...:	c0 0a 00 01 	sw \$0,1\(\$0\)
 ...:	c0 0a 00 00 	sw \$0,0\(\$0\)
 ...:	c0 0a 00 00 	sw \$0,0\(\$0\)
 ...:	c0 0a 00 01 	sw \$0,1\(\$0\)
 ...:	c0 0a 00 01 	sw \$0,1\(\$0\)
 ...:	c0 0c 00 01 	lb \$0,1\(\$0\)
 ...:	c0 0c 00 01 	lb \$0,1\(\$0\)
 ...:	c0 0c 00 00 	lb \$0,0\(\$0\)
 ...:	c0 0c 00 00 	lb \$0,0\(\$0\)
 ...:	c0 0c 00 01 	lb \$0,1\(\$0\)
 ...:	c0 0c 00 01 	lb \$0,1\(\$0\)
 ...:	c0 0d 00 01 	lh \$0,1\(\$0\)
 ...:	c0 0d 00 01 	lh \$0,1\(\$0\)
 ...:	c0 0d 00 00 	lh \$0,0\(\$0\)
 ...:	c0 0d 00 00 	lh \$0,0\(\$0\)
 ...:	c0 0d 00 01 	lh \$0,1\(\$0\)
 ...:	c0 0d 00 01 	lh \$0,1\(\$0\)
 ...:	c0 0e 00 01 	lw \$0,1\(\$0\)
 ...:	c0 0e 00 01 	lw \$0,1\(\$0\)
 ...:	c0 0e 00 00 	lw \$0,0\(\$0\)
 ...:	c0 0e 00 00 	lw \$0,0\(\$0\)
 ...:	c0 0e 00 01 	lw \$0,1\(\$0\)
 ...:	c0 0e 00 01 	lw \$0,1\(\$0\)
 ...:	c0 0b 00 01 	lbu \$0,1\(\$0\)
 ...:	c0 0b 00 01 	lbu \$0,1\(\$0\)
 ...:	c0 0b 00 00 	lbu \$0,0\(\$0\)
 ...:	c0 0b 00 00 	lbu \$0,0\(\$0\)
 ...:	c0 0b 00 01 	lbu \$0,1\(\$0\)
 ...:	c0 0b 00 01 	lbu \$0,1\(\$0\)
 ...:	c0 0f 00 01 	lhu \$0,1\(\$0\)
 ...:	c0 0f 00 01 	lhu \$0,1\(\$0\)
 ...:	c0 0f 00 00 	lhu \$0,0\(\$0\)
 ...:	c0 0f 00 00 	lhu \$0,0\(\$0\)
 ...:	c0 0f 00 01 	lhu \$0,1\(\$0\)
 ...:	c0 0f 00 01 	lhu \$0,1\(\$0\)
 ...:	c0 08 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 08 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 08 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 08 00 00 	sb \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	c0 09 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 09 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 09 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 09 00 00 	sh \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	c0 0a 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 0a 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 0a 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 0a 00 00 	sw \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	c0 0c 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 0c 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 0c 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 0c 00 00 	lb \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	c0 0d 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 0d 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 0d 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 0d 00 00 	lh \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	c0 0e 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 0e 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 0e 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 0e 00 00 	lw \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	c0 0b 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 0b 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 0b 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 0b 00 00 	lbu \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
 ...:	c0 0f 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_16	.text\+0x...
 ...:	c0 0f 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_LOW16	.text\+0x...
 ...:	c0 0f 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_HI16S	.text\+0x...
 ...:	c0 0f 00 00 	lhu \$0,0\(\$0\)
			...: R_MEP_HI16U	.text\+0x...
