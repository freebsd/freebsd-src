#as: -m10
#objdump: -drz
#name: q10test6

.*: +file format .*

Disassembly of section .text:

00000000 <_start>:
   0:	03 de f0 20 	add r30,r30,r30
   4:	00 00 00 00 	nop
   8:	23 de 00 00 	addi r30,r30,0x0
   c:	00 00 00 00 	nop
  10:	27 de 00 00 	addiu r30,r30,0x0
  14:	00 00 00 00 	nop
  18:	03 de f0 21 	addu r30,r30,r30
  1c:	00 00 00 00 	nop
  20:	03 de f0 29 	ado16 r30,r30,r30
  24:	00 00 00 00 	nop
  28:	03 de f0 24 	and r30,r30,r30
  2c:	00 00 00 00 	nop
  30:	33 de 00 00 	andi r30,r30,0x0
  34:	00 00 00 00 	nop
  38:	b3 de 00 00 	andoi r30,r30,0x0
  3c:	00 00 00 00 	nop
  40:	bf de 00 00 	andoui r30,r30,0x0
  44:	00 00 00 00 	nop
  48:	3c 1e 00 00 	lui r30,0x0
  4c:	00 00 00 00 	nop
  50:	03 de f0 2d 	mrgb r30,r30,r30,0x0
  54:	00 00 00 00 	nop
  58:	03 de f0 27 	nor r30,r30,r30
  5c:	00 00 00 00 	nop
  60:	03 de f0 25 	or r30,r30,r30
  64:	00 00 00 00 	nop
  68:	37 de 00 00 	ori r30,r30,0x0
  6c:	00 00 00 00 	nop
  70:	3f de 00 00 	orui r30,r30,0x0
  74:	00 00 00 00 	nop
  78:	00 1e f0 00 	sll r30,r30,0x0
  7c:	00 00 00 00 	nop
  80:	03 de f0 04 	sllv r30,r30,r30
  84:	00 00 00 00 	nop
  88:	03 de f0 2a 	slt r30,r30,r30
  8c:	00 00 00 00 	nop
  90:	2b de 00 00 	slti r30,r30,0x0
  94:	00 00 00 00 	nop
  98:	2f de 00 00 	sltiu r30,r30,0x0
  9c:	00 00 00 00 	nop
  a0:	03 de f0 2b 	sltu r30,r30,r30
  a4:	00 00 00 00 	nop
  a8:	00 1e f0 03 	sra r30,r30,0x0
  ac:	00 00 00 00 	nop
  b0:	03 de f0 07 	srav r30,r30,r30
  b4:	00 00 00 00 	nop
  b8:	00 1e f0 02 	srl r30,r30,0x0
  bc:	00 00 00 00 	nop
  c0:	03 de f0 06 	srlv r30,r30,r30
  c4:	00 00 00 00 	nop
  c8:	03 de f0 22 	sub r30,r30,r30
  cc:	00 00 00 00 	nop
  d0:	03 de f0 23 	subu r30,r30,r30
  d4:	00 00 00 00 	nop
  d8:	03 de f0 26 	xor r30,r30,r30
  dc:	00 00 00 00 	nop
  e0:	3b de 00 00 	xori r30,r30,0x0
  e4:	00 00 00 00 	nop
  e8:	00 00 00 00 	nop
  ec:	00 00 00 00 	nop
  f0:	03 de f0 05 	srmv r30,r30,r30,0x0
  f4:	00 00 00 00 	nop
  f8:	03 de f0 01 	slmv r30,r30,r30,0x0
  fc:	00 00 00 00 	nop
 100:	9c 1e f0 00 	ram r30,r30,0x0,0x0,0x0
 104:	00 00 00 00 	nop
 108:	73 c0 ff bd 	bbi r30\(0x0\),0 <_start>
 10c:	00 00 00 00 	nop
 110:	7b c0 ff bb 	bbin r30\(0x0\),0 <_start>
 114:	00 00 00 00 	nop
 118:	77 de ff b9 	bbv r30,r30,0 <_start>
 11c:	00 00 00 00 	nop
 120:	7f de ff b7 	bbvn r30,r30,0 <_start>
 124:	00 00 00 00 	nop
 128:	f3 c0 ff b5 	bbil r30\(0x0\),0 <_start>
 12c:	00 00 00 00 	nop
 130:	fb c0 ff b3 	bbinl r30\(0x0\),0 <_start>
 134:	00 00 00 00 	nop
 138:	f7 de ff b1 	bbvl r30,r30,0 <_start>
 13c:	00 00 00 00 	nop
 140:	ff de ff af 	bbvnl r30,r30,0 <_start>
 144:	00 00 00 00 	nop
 148:	13 de ff ad 	beq r30,r30,0 <_start>
 14c:	00 00 00 00 	nop
 150:	53 de ff ab 	beql r30,r30,0 <_start>
 154:	00 00 00 00 	nop
 158:	07 c1 ff a9 	bgez r30,0 <_start>
 15c:	00 00 00 00 	nop
 160:	07 d5 ff a7 	bgtzal r30,0 <_start>
 164:	00 00 00 00 	nop
 168:	07 d1 ff a5 	bgezal r30,0 <_start>
 16c:	00 00 00 00 	nop
 170:	07 d7 ff a3 	bgtzall r30,0 <_start>
 174:	00 00 00 00 	nop
 178:	07 d3 ff a1 	bgezall r30,0 <_start>
 17c:	00 00 00 00 	nop
 180:	07 c3 ff 9f 	bgezl r30,0 <_start>
 184:	00 00 00 00 	nop
 188:	07 c7 ff 9d 	bgtzl r30,0 <_start>
 18c:	00 00 00 00 	nop
 190:	07 c5 ff 9b 	bgtz r30,0 <_start>
 194:	00 00 00 00 	nop
 198:	07 c4 ff 99 	blez r30,0 <_start>
 19c:	00 00 00 00 	nop
 1a0:	07 d4 ff 97 	blezal r30,0 <_start>
 1a4:	00 00 00 00 	nop
 1a8:	07 c0 ff 95 	bltz r30,0 <_start>
 1ac:	00 00 00 00 	nop
 1b0:	07 d0 ff 93 	bltzal r30,0 <_start>
 1b4:	00 00 00 00 	nop
 1b8:	07 c6 ff 91 	blezl r30,0 <_start>
 1bc:	00 00 00 00 	nop
 1c0:	07 c2 ff 8f 	bltzl r30,0 <_start>
 1c4:	00 00 00 00 	nop
 1c8:	07 d6 ff 8d 	blezall r30,0 <_start>
 1cc:	00 00 00 00 	nop
 1d0:	07 d2 ff 8b 	bltzall r30,0 <_start>
 1d4:	00 00 00 00 	nop
 1d8:	1b de ff 89 	bmb r30,r30,0 <_start>
 1dc:	00 00 00 00 	nop
 1e0:	5b de ff 87 	bmbl r30,r30,0 <_start>
 1e4:	00 00 00 00 	nop
 1e8:	63 de ff 85 	bmb0 r30,r30,0 <_start>
 1ec:	00 00 00 00 	nop
 1f0:	67 de ff 83 	bmb1 r30,r30,0 <_start>
 1f4:	00 00 00 00 	nop
 1f8:	6b de ff 81 	bmb2 r30,r30,0 <_start>
 1fc:	00 00 00 00 	nop
 200:	6f de ff 7f 	bmb3 r30,r30,0 <_start>
 204:	00 00 00 00 	nop
 208:	17 de ff 7d 	bne r30,r30,0 <_start>
 20c:	00 00 00 00 	nop
 210:	57 de ff 7b 	bnel r30,r30,0 <_start>
 214:	00 00 00 00 	nop
 218:	08 00 00 00 	j 0 <_start>
 21c:	00 00 00 00 	nop
 220:	0c 1e 00 00 	jal r30,0 <_start>
 224:	00 00 00 00 	nop
 228:	03 c0 f0 09 	jalr r30,r30
 22c:	00 00 00 00 	nop
 230:	03 c0 00 08 	jr r30
 234:	00 00 00 00 	nop
 238:	00 00 00 0d 	break
 23c:	00 00 00 00 	nop
 240:	4f de 00 02 	ctc r30,r30
 244:	00 00 00 00 	nop
 248:	4c 1e f0 00 	cfc r30,r30
 24c:	00 00 00 00 	nop
 250:	8f de 00 00 	lw r30,0x0\(r30\)
 254:	00 00 00 00 	nop
 258:	87 de 00 00 	lh r30,0x0\(r30\)
 25c:	00 00 00 00 	nop
 260:	83 de 00 00 	lb r30,0x0\(r30\)
 264:	00 00 00 00 	nop
 268:	97 de 00 00 	lhu r30,0x0\(r30\)
 26c:	00 00 00 00 	nop
 270:	93 de 00 00 	lbu r30,0x0\(r30\)
 274:	00 00 00 00 	nop
 278:	a3 de 00 00 	sb r30,0x0\(r30\)
 27c:	00 00 00 00 	nop
 280:	a7 de 00 00 	sh r30,0x0\(r30\)
 284:	00 00 00 00 	nop
 288:	af de 00 00 	sw r30,0x0\(r30\)
 28c:	00 00 00 00 	nop
 290:	4f de f0 08 	rba r30,r30,r30
 294:	00 00 00 00 	nop
 298:	4f de f0 0a 	rbar r30,r30,r30
 29c:	00 00 00 00 	nop
 2a0:	4f de f0 09 	rbal r30,r30,r30
 2a4:	00 00 00 00 	nop
 2a8:	4f de f0 10 	wba r30,r30,r30
 2ac:	00 00 00 00 	nop
 2b0:	4f de f0 12 	wbac r30,r30,r30
 2b4:	00 00 00 00 	nop
 2b8:	4f de f0 11 	wbau r30,r30,r30
 2bc:	00 00 00 00 	nop
 2c0:	4f de f2 00 	rbi r30,r30,r30,0x0
 2c4:	00 00 00 00 	nop
 2c8:	4f de f1 00 	rbir r30,r30,r30,0x0
 2cc:	00 00 00 00 	nop
 2d0:	4f de f3 00 	rbil r30,r30,r30,0x0
 2d4:	00 00 00 00 	nop
 2d8:	4f de f6 00 	wbi r30,r30,r30,0x0
 2dc:	00 00 00 00 	nop
 2e0:	4f de f5 00 	wbic r30,r30,r30,0x0
 2e4:	00 00 00 00 	nop
 2e8:	4f de f7 00 	wbiu r30,r30,r30,0x0
 2ec:	00 00 00 00 	nop
 2f0:	4f de f0 28 	pkrla r30,r30,r30
 2f4:	00 00 00 00 	nop
 2f8:	4f de f0 2a 	pkrlah r30,r30,r30
 2fc:	00 00 00 00 	nop
 300:	4f de f0 29 	pkrlau r30,r30,r30
 304:	00 00 00 00 	nop
 308:	4b de f0 00 	pkrli r30,r30,r30,0x0
 30c:	00 00 00 00 	nop
 310:	4b de f2 00 	pkrlih r30,r30,r30,0x0
 314:	00 00 00 00 	nop
 318:	4b de f1 00 	pkrliu r30,r30,r30,0x0
 31c:	00 00 00 00 	nop
 320:	4c 1e f0 01 	lock r30,r30
 324:	00 00 00 00 	nop
 328:	4c 1e f0 03 	unlk r30,r30
 32c:	00 00 00 00 	nop
 330:	4f de f0 06 	swwr r30,r30,r30
 334:	00 00 00 00 	nop
 338:	4f de f0 07 	swwru r30,r30,r30
 33c:	00 00 00 00 	nop
 340:	4c 1e f0 04 	swrd r30,r30
 344:	00 00 00 00 	nop
 348:	4c 1e f0 05 	swrdl r30,r30
 34c:	00 00 00 00 	nop
 350:	4c 1e f0 0c 	dwrd r30,r30
 354:	00 00 00 00 	nop
 358:	4c 1e f0 0d 	dwrdl r30,r30
 35c:	00 00 00 00 	nop
 360:	4c 1e f4 30 	cam36 r30,r30,0x6,0x0
 364:	00 00 00 00 	nop
 368:	4c 1e f4 46 	cam72 r30,r30,0x6,0x0
 36c:	00 00 00 00 	nop
 370:	4c 1e f4 86 	cam144 r30,r30,0x6,0x0
 374:	00 00 00 00 	nop
 378:	4c 1e f4 c6 	cam288 r30,r30,0x6,0x0
 37c:	00 00 00 00 	nop
 380:	4f de f0 ab 	cm32and r30,r30,r30
 384:	00 00 00 00 	nop
 388:	4f de f0 a3 	cm32andn r30,r30,r30
 38c:	00 00 00 00 	nop
 390:	4f de f0 aa 	cm32or r30,r30,r30
 394:	00 00 00 00 	nop
 398:	4f de f0 b0 	cm32ra r30,r30,r30
 39c:	00 00 00 00 	nop
 3a0:	4c 1e f0 a1 	cm32rd r30,r30
 3a4:	00 00 00 00 	nop
 3a8:	4c 1e f0 a4 	cm32ri r30,r30
 3ac:	00 00 00 00 	nop
 3b0:	4f de f0 a0 	cm32rs r30,r30,r30
 3b4:	00 00 00 00 	nop
 3b8:	4f de f0 b8 	cm32sa r30,r30,r30
 3bc:	00 00 00 00 	nop
 3c0:	4c 1e f0 a9 	cm32sd r30,r30
 3c4:	00 00 00 00 	nop
 3c8:	4c 1e f0 ac 	cm32si r30,r30
 3cc:	00 00 00 00 	nop
 3d0:	4f de f0 a8 	cm32ss r30,r30,r30
 3d4:	00 00 00 00 	nop
 3d8:	4f de f0 a2 	cm32xor r30,r30,r30
 3dc:	00 00 00 00 	nop
 3e0:	4c 1e f0 85 	cm64clr r30,r30
 3e4:	00 00 00 00 	nop
 3e8:	4f de f0 90 	cm64ra r30,r30,r30
 3ec:	00 00 00 00 	nop
 3f0:	4c 1e f0 81 	cm64rd r30,r30
 3f4:	00 00 00 00 	nop
 3f8:	4c 1e f0 84 	cm64ri r30,r30
 3fc:	00 00 00 00 	nop
 400:	4f de f0 94 	cm64ria2 r30,r30,r30
 404:	00 00 00 00 	nop
 408:	4f de f0 80 	cm64rs r30,r30,r30
 40c:	00 00 00 00 	nop
 410:	4f de f0 98 	cm64sa r30,r30,r30
 414:	00 00 00 00 	nop
 418:	4c 1e f0 89 	cm64sd r30,r30
 41c:	00 00 00 00 	nop
 420:	4c 1e f0 8c 	cm64si r30,r30
 424:	00 00 00 00 	nop
 428:	4f de f0 9c 	cm64sia2 r30,r30,r30
 42c:	00 00 00 00 	nop
 430:	4f de f0 88 	cm64ss r30,r30,r30
 434:	00 00 00 00 	nop
 438:	4f de f0 95 	cm128ria2 r30,r30,r30
 43c:	00 00 00 00 	nop
 440:	4f de f0 93 	cm128ria3 r30,r30,r30,0x3
 444:	00 00 00 00 	nop
 448:	4f de f0 b6 	cm128ria4 r30,r30,r30,0x6
 44c:	00 00 00 00 	nop
 450:	4f de f0 9d 	cm128sia2 r30,r30,r30
 454:	00 00 00 00 	nop
 458:	4f de f0 9b 	cm128sia3 r30,r30,r30,0x3
 45c:	00 00 00 00 	nop
 460:	4f de f0 be 	cm128sia4 r30,r30,r30,0x6
 464:	00 00 00 00 	nop
 468:	4f de f0 a6 	cm128vsa r30,r30,r30
 46c:	00 00 00 00 	nop
 470:	4f de f0 14 	crc32 r30,r30,r30
 474:	00 00 00 00 	nop
 478:	4f de f0 15 	crc32b r30,r30,r30
 47c:	00 00 00 00 	nop
 480:	4f c0 f0 26 	chkhdr r30,r30
 484:	00 00 00 00 	nop
 488:	4c 00 f0 24 	avail r30
 48c:	00 00 00 00 	nop
 490:	4f c0 f0 25 	free r30,r30
 494:	00 00 00 00 	nop
 498:	4f c0 f0 27 	tstod r30,r30
 49c:	00 00 00 00 	nop
 4a0:	4c 00 f0 2c 	cmphdr r30
 4a4:	00 00 00 00 	nop
 4a8:	4c 1e f0 20 	mcid r30,r30
 4ac:	00 00 00 00 	nop
 4b0:	4c 00 f0 22 	dba r30
 4b4:	00 00 00 00 	nop
 4b8:	4c 1e f0 21 	dbd r30,r0,r30
 4bc:	00 00 00 00 	nop
 4c0:	4f c0 f0 23 	dpwt r30,r30
 4c4:	00 00 00 00 	nop
