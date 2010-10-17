#as: -m10
#objdump: -drz
#name: q10test2

.*: +file format .*

Disassembly of section .text:

00000000 <_start>:
   0:	00 21 f8 20 	add r31,r1,r1
   4:	00 00 00 00 	nop
   8:	20 21 00 00 	addi r1,r1,0x0
   c:	00 00 00 00 	nop
  10:	24 21 00 00 	addiu r1,r1,0x0
  14:	00 00 00 00 	nop
  18:	00 21 f8 21 	addu r31,r1,r1
  1c:	00 00 00 00 	nop
  20:	00 21 f8 29 	ado16 r31,r1,r1
  24:	00 00 00 00 	nop
  28:	00 21 f8 24 	and r31,r1,r1
  2c:	00 00 00 00 	nop
  30:	30 21 00 00 	andi r1,r1,0x0
  34:	00 00 00 00 	nop
  38:	b0 21 00 00 	andoi r1,r1,0x0
  3c:	00 00 00 00 	nop
  40:	bc 21 00 00 	andoui r1,r1,0x0
  44:	00 00 00 00 	nop
  48:	3c 01 00 00 	lui r1,0x0
  4c:	00 00 00 00 	nop
  50:	00 21 f8 2d 	mrgb r31,r1,r1,0x0
  54:	00 00 00 00 	nop
  58:	00 21 f8 27 	nor r31,r1,r1
  5c:	00 00 00 00 	nop
  60:	00 21 f8 25 	or r31,r1,r1
  64:	00 00 00 00 	nop
  68:	34 21 00 00 	ori r1,r1,0x0
  6c:	00 00 00 00 	nop
  70:	3c 21 00 00 	orui r1,r1,0x0
  74:	00 00 00 00 	nop
  78:	00 01 f8 00 	sll r31,r1,0x0
  7c:	00 00 00 00 	nop
  80:	00 21 f8 04 	sllv r31,r1,r1
  84:	00 00 00 00 	nop
  88:	00 21 f8 2a 	slt r31,r1,r1
  8c:	00 00 00 00 	nop
  90:	28 21 00 00 	slti r1,r1,0x0
  94:	00 00 00 00 	nop
  98:	2c 21 00 00 	sltiu r1,r1,0x0
  9c:	00 00 00 00 	nop
  a0:	00 21 f8 2b 	sltu r31,r1,r1
  a4:	00 00 00 00 	nop
  a8:	00 01 f8 03 	sra r31,r1,0x0
  ac:	00 00 00 00 	nop
  b0:	00 21 f8 07 	srav r31,r1,r1
  b4:	00 00 00 00 	nop
  b8:	00 01 f8 02 	srl r31,r1,0x0
  bc:	00 00 00 00 	nop
  c0:	00 21 f8 06 	srlv r31,r1,r1
  c4:	00 00 00 00 	nop
  c8:	00 21 f8 22 	sub r31,r1,r1
  cc:	00 00 00 00 	nop
  d0:	00 21 f8 23 	subu r31,r1,r1
  d4:	00 00 00 00 	nop
  d8:	00 21 f8 26 	xor r31,r1,r1
  dc:	00 00 00 00 	nop
  e0:	38 21 00 00 	xori r1,r1,0x0
  e4:	00 00 00 00 	nop
  e8:	00 00 00 00 	nop
  ec:	00 00 00 00 	nop
  f0:	00 21 f8 05 	srmv r31,r1,r1,0x0
  f4:	00 00 00 00 	nop
  f8:	00 21 f8 01 	slmv r31,r1,r1,0x0
  fc:	00 00 00 00 	nop
 100:	9c 01 f8 00 	ram r31,r1,0x0,0x0,0x0
 104:	00 00 00 00 	nop
 108:	70 20 ff bd 	bbi r1\(0x0\),0 <_start>
 10c:	00 00 00 00 	nop
 110:	78 20 ff bb 	bbin r1\(0x0\),0 <_start>
 114:	00 00 00 00 	nop
 118:	74 21 ff b9 	bbv r1,r1,0 <_start>
 11c:	00 00 00 00 	nop
 120:	7c 21 ff b7 	bbvn r1,r1,0 <_start>
 124:	00 00 00 00 	nop
 128:	f0 20 ff b5 	bbil r1\(0x0\),0 <_start>
 12c:	00 00 00 00 	nop
 130:	f8 20 ff b3 	bbinl r1\(0x0\),0 <_start>
 134:	00 00 00 00 	nop
 138:	f4 21 ff b1 	bbvl r1,r1,0 <_start>
 13c:	00 00 00 00 	nop
 140:	fc 21 ff af 	bbvnl r1,r1,0 <_start>
 144:	00 00 00 00 	nop
 148:	10 21 ff ad 	beq r1,r1,0 <_start>
 14c:	00 00 00 00 	nop
 150:	50 21 ff ab 	beql r1,r1,0 <_start>
 154:	00 00 00 00 	nop
 158:	04 21 ff a9 	bgez r1,0 <_start>
 15c:	00 00 00 00 	nop
 160:	04 35 ff a7 	bgtzal r1,0 <_start>
 164:	00 00 00 00 	nop
 168:	04 31 ff a5 	bgezal r1,0 <_start>
 16c:	00 00 00 00 	nop
 170:	04 37 ff a3 	bgtzall r1,0 <_start>
 174:	00 00 00 00 	nop
 178:	04 33 ff a1 	bgezall r1,0 <_start>
 17c:	00 00 00 00 	nop
 180:	04 23 ff 9f 	bgezl r1,0 <_start>
 184:	00 00 00 00 	nop
 188:	04 27 ff 9d 	bgtzl r1,0 <_start>
 18c:	00 00 00 00 	nop
 190:	04 25 ff 9b 	bgtz r1,0 <_start>
 194:	00 00 00 00 	nop
 198:	04 24 ff 99 	blez r1,0 <_start>
 19c:	00 00 00 00 	nop
 1a0:	04 34 ff 97 	blezal r1,0 <_start>
 1a4:	00 00 00 00 	nop
 1a8:	04 20 ff 95 	bltz r1,0 <_start>
 1ac:	00 00 00 00 	nop
 1b0:	04 30 ff 93 	bltzal r1,0 <_start>
 1b4:	00 00 00 00 	nop
 1b8:	04 26 ff 91 	blezl r1,0 <_start>
 1bc:	00 00 00 00 	nop
 1c0:	04 22 ff 8f 	bltzl r1,0 <_start>
 1c4:	00 00 00 00 	nop
 1c8:	04 36 ff 8d 	blezall r1,0 <_start>
 1cc:	00 00 00 00 	nop
 1d0:	04 32 ff 8b 	bltzall r1,0 <_start>
 1d4:	00 00 00 00 	nop
 1d8:	18 21 ff 89 	bmb r1,r1,0 <_start>
 1dc:	00 00 00 00 	nop
 1e0:	58 21 ff 87 	bmbl r1,r1,0 <_start>
 1e4:	00 00 00 00 	nop
 1e8:	60 21 ff 85 	bmb0 r1,r1,0 <_start>
 1ec:	00 00 00 00 	nop
 1f0:	64 21 ff 83 	bmb1 r1,r1,0 <_start>
 1f4:	00 00 00 00 	nop
 1f8:	68 21 ff 81 	bmb2 r1,r1,0 <_start>
 1fc:	00 00 00 00 	nop
 200:	6c 21 ff 7f 	bmb3 r1,r1,0 <_start>
 204:	00 00 00 00 	nop
 208:	14 21 ff 7d 	bne r1,r1,0 <_start>
 20c:	00 00 00 00 	nop
 210:	54 21 ff 7b 	bnel r1,r1,0 <_start>
 214:	00 00 00 00 	nop
 218:	08 00 00 00 	j 0 <_start>
 21c:	00 00 00 00 	nop
 220:	0c 01 00 00 	jal r1,0 <_start>
 224:	00 00 00 00 	nop
 228:	00 20 f8 09 	jalr r31,r1
 22c:	00 00 00 00 	nop
 230:	00 20 00 08 	jr r1
 234:	00 00 00 00 	nop
 238:	00 00 00 0d 	break
 23c:	00 00 00 00 	nop
 240:	4c 21 00 02 	ctc r1,r1
 244:	00 00 00 00 	nop
 248:	4c 01 f8 00 	cfc r31,r1
 24c:	00 00 00 00 	nop
 250:	8c 21 00 00 	lw r1,0x0\(r1\)
 254:	00 00 00 00 	nop
 258:	84 21 00 00 	lh r1,0x0\(r1\)
 25c:	00 00 00 00 	nop
 260:	80 21 00 00 	lb r1,0x0\(r1\)
 264:	00 00 00 00 	nop
 268:	94 21 00 00 	lhu r1,0x0\(r1\)
 26c:	00 00 00 00 	nop
 270:	90 21 00 00 	lbu r1,0x0\(r1\)
 274:	00 00 00 00 	nop
 278:	a0 21 00 00 	sb r1,0x0\(r1\)
 27c:	00 00 00 00 	nop
 280:	a4 21 00 00 	sh r1,0x0\(r1\)
 284:	00 00 00 00 	nop
 288:	ac 21 00 00 	sw r1,0x0\(r1\)
 28c:	00 00 00 00 	nop
 290:	4c 3f 08 08 	rba r1,r1,r31
 294:	00 00 00 00 	nop
 298:	4c 3f 08 0a 	rbar r1,r1,r31
 29c:	00 00 00 00 	nop
 2a0:	4c 3f 08 09 	rbal r1,r1,r31
 2a4:	00 00 00 00 	nop
 2a8:	4c 3f 08 10 	wba r1,r1,r31
 2ac:	00 00 00 00 	nop
 2b0:	4c 3f 08 12 	wbac r1,r1,r31
 2b4:	00 00 00 00 	nop
 2b8:	4c 3f 08 11 	wbau r1,r1,r31
 2bc:	00 00 00 00 	nop
 2c0:	4c 3f 0a 00 	rbi r1,r1,r31,0x0
 2c4:	00 00 00 00 	nop
 2c8:	4c 3f 09 00 	rbir r1,r1,r31,0x0
 2cc:	00 00 00 00 	nop
 2d0:	4c 3f 0b 00 	rbil r1,r1,r31,0x0
 2d4:	00 00 00 00 	nop
 2d8:	4c 3f 0e 00 	wbi r1,r1,r31,0x0
 2dc:	00 00 00 00 	nop
 2e0:	4c 3f 0d 00 	wbic r1,r1,r31,0x0
 2e4:	00 00 00 00 	nop
 2e8:	4c 3f 0f 00 	wbiu r1,r1,r31,0x0
 2ec:	00 00 00 00 	nop
 2f0:	4c 3f 08 28 	pkrla r1,r1,r31
 2f4:	00 00 00 00 	nop
 2f8:	4c 3f 08 2b 	pkrlac r1,r1,r31
 2fc:	00 00 00 00 	nop
 300:	4c 3f 08 2a 	pkrlah r1,r1,r31
 304:	00 00 00 00 	nop
 308:	4c 3f 08 29 	pkrlau r1,r1,r31
 30c:	00 00 00 00 	nop
 310:	48 3f 08 00 	pkrli r1,r1,r31,0x0
 314:	00 00 00 00 	nop
 318:	48 3f 0b 00 	pkrlic r1,r1,r31,0x0
 31c:	00 00 00 00 	nop
 320:	48 3f 0a 00 	pkrlih r1,r1,r31,0x0
 324:	00 00 00 00 	nop
 328:	48 3f 09 00 	pkrliu r1,r1,r31,0x0
 32c:	00 00 00 00 	nop
 330:	4c 1f 08 01 	lock r1,r31
 334:	00 00 00 00 	nop
 338:	4c 1f 08 03 	unlk r1,r31
 33c:	00 00 00 00 	nop
 340:	4c 3f 08 06 	swwr r1,r1,r31
 344:	00 00 00 00 	nop
 348:	4c 3f 08 07 	swwru r1,r1,r31
 34c:	00 00 00 00 	nop
 350:	4c 01 f8 04 	swrd r31,r1
 354:	00 00 00 00 	nop
 358:	4c 01 f8 05 	swrdl r31,r1
 35c:	00 00 00 00 	nop
 360:	4c 02 f0 0c 	dwrd r30,r2
 364:	00 00 00 00 	nop
 368:	4c 02 f0 0d 	dwrdl r30,r2
 36c:	00 00 00 00 	nop
 370:	4c 01 fc 10 	cam36 r31,r1,0x2,0x0
 374:	00 00 00 00 	nop
 378:	4c 01 fc 42 	cam72 r31,r1,0x2,0x0
 37c:	00 00 00 00 	nop
 380:	4c 01 fc 82 	cam144 r31,r1,0x2,0x0
 384:	00 00 00 00 	nop
 388:	4c 01 fc c2 	cam288 r31,r1,0x2,0x0
 38c:	00 00 00 00 	nop
 390:	4c 21 f8 ab 	cm32and r31,r1,r1
 394:	00 00 00 00 	nop
 398:	4c 21 f8 a3 	cm32andn r31,r1,r1
 39c:	00 00 00 00 	nop
 3a0:	4c 21 f8 aa 	cm32or r31,r1,r1
 3a4:	00 00 00 00 	nop
 3a8:	4c 21 f8 b0 	cm32ra r31,r1,r1
 3ac:	00 00 00 00 	nop
 3b0:	4c 01 f8 a1 	cm32rd r31,r1
 3b4:	00 00 00 00 	nop
 3b8:	4c 01 f8 a4 	cm32ri r31,r1
 3bc:	00 00 00 00 	nop
 3c0:	4c 21 f8 a0 	cm32rs r31,r1,r1
 3c4:	00 00 00 00 	nop
 3c8:	4c 21 f8 b8 	cm32sa r31,r1,r1
 3cc:	00 00 00 00 	nop
 3d0:	4c 01 f8 a9 	cm32sd r31,r1
 3d4:	00 00 00 00 	nop
 3d8:	4c 01 f8 ac 	cm32si r31,r1
 3dc:	00 00 00 00 	nop
 3e0:	4c 21 f8 a8 	cm32ss r31,r1,r1
 3e4:	00 00 00 00 	nop
 3e8:	4c 21 f8 a2 	cm32xor r31,r1,r1
 3ec:	00 00 00 00 	nop
 3f0:	4c 02 f0 85 	cm64clr r30,r2
 3f4:	00 00 00 00 	nop
 3f8:	4c 42 f0 90 	cm64ra r30,r2,r2
 3fc:	00 00 00 00 	nop
 400:	4c 02 f0 81 	cm64rd r30,r2
 404:	00 00 00 00 	nop
 408:	4c 02 f0 84 	cm64ri r30,r2
 40c:	00 00 00 00 	nop
 410:	4c 42 f0 94 	cm64ria2 r30,r2,r2
 414:	00 00 00 00 	nop
 418:	4c 42 f0 80 	cm64rs r30,r2,r2
 41c:	00 00 00 00 	nop
 420:	4c 42 f0 98 	cm64sa r30,r2,r2
 424:	00 00 00 00 	nop
 428:	4c 02 f0 89 	cm64sd r30,r2
 42c:	00 00 00 00 	nop
 430:	4c 02 f0 8c 	cm64si r30,r2
 434:	00 00 00 00 	nop
 438:	4c 42 f0 9c 	cm64sia2 r30,r2,r2
 43c:	00 00 00 00 	nop
 440:	4c 42 f0 88 	cm64ss r30,r2,r2
 444:	00 00 00 00 	nop
 448:	4c 42 f0 95 	cm128ria2 r30,r2,r2
 44c:	00 00 00 00 	nop
 450:	4c 42 f0 92 	cm128ria3 r30,r2,r2,0x2
 454:	00 00 00 00 	nop
 458:	4c 42 f0 b2 	cm128ria4 r30,r2,r2,0x2
 45c:	00 00 00 00 	nop
 460:	4c 42 f0 9d 	cm128sia2 r30,r2,r2
 464:	00 00 00 00 	nop
 468:	4c 42 f0 9a 	cm128sia3 r30,r2,r2,0x2
 46c:	00 00 00 00 	nop
 470:	4c 21 f8 ba 	cm128sia4 r31,r1,r1,0x2
 474:	00 00 00 00 	nop
 478:	4c 21 f8 a6 	cm128vsa r31,r1,r1
 47c:	00 00 00 00 	nop
 480:	4c 21 f8 14 	crc32 r31,r1,r1
 484:	00 00 00 00 	nop
 488:	4c 21 f8 15 	crc32b r31,r1,r1
 48c:	00 00 00 00 	nop
 490:	4c 20 f8 26 	chkhdr r31,r1
 494:	00 00 00 00 	nop
 498:	4c 00 f8 24 	avail r31
 49c:	00 00 00 00 	nop
 4a0:	4c 20 f8 25 	free r31,r1
 4a4:	00 00 00 00 	nop
 4a8:	4c 20 f8 27 	tstod r31,r1
 4ac:	00 00 00 00 	nop
 4b0:	4c 00 f8 2c 	cmphdr r31
 4b4:	00 00 00 00 	nop
 4b8:	4c 01 f8 20 	mcid r31,r1
 4bc:	00 00 00 00 	nop
 4c0:	4c 00 f8 22 	dba r31
 4c4:	00 00 00 00 	nop
 4c8:	4c 1f 08 21 	dbd r1,r0,r31
 4cc:	00 00 00 00 	nop
 4d0:	4f e0 08 23 	dpwt r1,r31
 4d4:	00 00 00 00 	nop
