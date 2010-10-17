#as: -m10
#objdump: -drz
#name: q10test1

.*: +file format .*

Disassembly of section .text:

00000000 <_start>:
   0:	03 e1 08 20 	add r1,r31,r1
   4:	00 00 00 00 	nop
   8:	23 e1 00 00 	addi r1,r31,0x0
   c:	00 00 00 00 	nop
  10:	27 e1 00 00 	addiu r1,r31,0x0
  14:	00 00 00 00 	nop
  18:	03 e1 08 21 	addu r1,r31,r1
  1c:	00 00 00 00 	nop
  20:	03 e1 08 29 	ado16 r1,r31,r1
  24:	00 00 00 00 	nop
  28:	03 e1 08 24 	and r1,r31,r1
  2c:	00 00 00 00 	nop
  30:	33 e1 00 00 	andi r1,r31,0x0
  34:	00 00 00 00 	nop
  38:	b3 e1 00 00 	andoi r1,r31,0x0
  3c:	00 00 00 00 	nop
  40:	bf e1 00 00 	andoui r1,r31,0x0
  44:	00 00 00 00 	nop
  48:	3c 01 00 00 	lui r1,0x0
  4c:	00 00 00 00 	nop
  50:	03 e1 08 2d 	mrgb r1,r31,r1,0x0
  54:	00 00 00 00 	nop
  58:	03 e1 08 27 	nor r1,r31,r1
  5c:	00 00 00 00 	nop
  60:	03 e1 08 25 	or r1,r31,r1
  64:	00 00 00 00 	nop
  68:	37 e1 00 00 	ori r1,r31,0x0
  6c:	00 00 00 00 	nop
  70:	3f e1 00 00 	orui r1,r31,0x0
  74:	00 00 00 00 	nop
  78:	00 01 08 00 	sll r1,r1,0x0
  7c:	00 00 00 00 	nop
  80:	03 e1 08 04 	sllv r1,r1,r31
  84:	00 00 00 00 	nop
  88:	03 e1 08 2a 	slt r1,r31,r1
  8c:	00 00 00 00 	nop
  90:	2b e1 00 00 	slti r1,r31,0x0
  94:	00 00 00 00 	nop
  98:	2f e1 00 00 	sltiu r1,r31,0x0
  9c:	00 00 00 00 	nop
  a0:	03 e1 08 2b 	sltu r1,r31,r1
  a4:	00 00 00 00 	nop
  a8:	00 01 08 03 	sra r1,r1,0x0
  ac:	00 00 00 00 	nop
  b0:	03 e1 08 07 	srav r1,r1,r31
  b4:	00 00 00 00 	nop
  b8:	00 01 08 02 	srl r1,r1,0x0
  bc:	00 00 00 00 	nop
  c0:	03 e1 08 06 	srlv r1,r1,r31
  c4:	00 00 00 00 	nop
  c8:	03 e1 08 22 	sub r1,r31,r1
  cc:	00 00 00 00 	nop
  d0:	03 e1 08 23 	subu r1,r31,r1
  d4:	00 00 00 00 	nop
  d8:	03 e1 08 26 	xor r1,r31,r1
  dc:	00 00 00 00 	nop
  e0:	3b e1 00 00 	xori r1,r31,0x0
  e4:	00 00 00 00 	nop
  e8:	00 00 00 00 	nop
  ec:	00 00 00 00 	nop
  f0:	00 3f 08 05 	srmv r1,r31,r1,0x0
  f4:	00 00 00 00 	nop
  f8:	00 3f 08 01 	slmv r1,r31,r1,0x0
  fc:	00 00 00 00 	nop
 100:	9c 01 08 00 	ram r1,r1,0x0,0x0,0x0
 104:	00 00 00 00 	nop
 108:	73 e0 ff bd 	bbi r31\(0x0\),0 <_start>
 10c:	00 00 00 00 	nop
 110:	7b e0 ff bb 	bbin r31\(0x0\),0 <_start>
 114:	00 00 00 00 	nop
 118:	77 e1 ff b9 	bbv r31,r1,0 <_start>
 11c:	00 00 00 00 	nop
 120:	7f e1 ff b7 	bbvn r31,r1,0 <_start>
 124:	00 00 00 00 	nop
 128:	f3 e0 ff b5 	bbil r31\(0x0\),0 <_start>
 12c:	00 00 00 00 	nop
 130:	fb e0 ff b3 	bbinl r31\(0x0\),0 <_start>
 134:	00 00 00 00 	nop
 138:	f7 e1 ff b1 	bbvl r31,r1,0 <_start>
 13c:	00 00 00 00 	nop
 140:	ff e1 ff af 	bbvnl r31,r1,0 <_start>
 144:	00 00 00 00 	nop
 148:	13 e1 ff ad 	beq r31,r1,0 <_start>
 14c:	00 00 00 00 	nop
 150:	53 e1 ff ab 	beql r31,r1,0 <_start>
 154:	00 00 00 00 	nop
 158:	07 e1 ff a9 	bgez r31,0 <_start>
 15c:	00 00 00 00 	nop
 160:	07 f5 ff a7 	bgtzal r31,0 <_start>
 164:	00 00 00 00 	nop
 168:	07 f1 ff a5 	bgezal r31,0 <_start>
 16c:	00 00 00 00 	nop
 170:	07 f7 ff a3 	bgtzall r31,0 <_start>
 174:	00 00 00 00 	nop
 178:	07 f3 ff a1 	bgezall r31,0 <_start>
 17c:	00 00 00 00 	nop
 180:	07 e3 ff 9f 	bgezl r31,0 <_start>
 184:	00 00 00 00 	nop
 188:	07 e7 ff 9d 	bgtzl r31,0 <_start>
 18c:	00 00 00 00 	nop
 190:	07 e5 ff 9b 	bgtz r31,0 <_start>
 194:	00 00 00 00 	nop
 198:	07 e4 ff 99 	blez r31,0 <_start>
 19c:	00 00 00 00 	nop
 1a0:	07 f4 ff 97 	blezal r31,0 <_start>
 1a4:	00 00 00 00 	nop
 1a8:	07 e0 ff 95 	bltz r31,0 <_start>
 1ac:	00 00 00 00 	nop
 1b0:	07 f0 ff 93 	bltzal r31,0 <_start>
 1b4:	00 00 00 00 	nop
 1b8:	07 e6 ff 91 	blezl r31,0 <_start>
 1bc:	00 00 00 00 	nop
 1c0:	07 e2 ff 8f 	bltzl r31,0 <_start>
 1c4:	00 00 00 00 	nop
 1c8:	07 f6 ff 8d 	blezall r31,0 <_start>
 1cc:	00 00 00 00 	nop
 1d0:	07 f2 ff 8b 	bltzall r31,0 <_start>
 1d4:	00 00 00 00 	nop
 1d8:	1b e1 ff 89 	bmb r31,r1,0 <_start>
 1dc:	00 00 00 00 	nop
 1e0:	5b e1 ff 87 	bmbl r31,r1,0 <_start>
 1e4:	00 00 00 00 	nop
 1e8:	63 e1 ff 85 	bmb0 r31,r1,0 <_start>
 1ec:	00 00 00 00 	nop
 1f0:	67 e1 ff 83 	bmb1 r31,r1,0 <_start>
 1f4:	00 00 00 00 	nop
 1f8:	6b e1 ff 81 	bmb2 r31,r1,0 <_start>
 1fc:	00 00 00 00 	nop
 200:	6f e1 ff 7f 	bmb3 r31,r1,0 <_start>
 204:	00 00 00 00 	nop
 208:	17 e1 ff 7d 	bne r31,r1,0 <_start>
 20c:	00 00 00 00 	nop
 210:	57 e1 ff 7b 	bnel r31,r1,0 <_start>
 214:	00 00 00 00 	nop
 218:	08 00 00 00 	j 0 <_start>
 21c:	00 00 00 00 	nop
 220:	0c 1f 00 00 	jal 0 <_start>
 224:	00 00 00 00 	nop
 228:	03 e0 08 09 	jalr r1,r31
 22c:	00 00 00 00 	nop
 230:	03 e0 00 08 	jr r31
 234:	00 00 00 00 	nop
 238:	00 00 00 0d 	break
 23c:	00 00 00 00 	nop
 240:	4f e1 00 02 	ctc r31,r1
 244:	00 00 00 00 	nop
 248:	4c 01 08 00 	cfc r1,r1
 24c:	00 00 00 00 	nop
 250:	8f e1 00 00 	lw r1,0x0\(r31\)
 254:	00 00 00 00 	nop
 258:	87 e1 00 00 	lh r1,0x0\(r31\)
 25c:	00 00 00 00 	nop
 260:	83 e1 00 00 	lb r1,0x0\(r31\)
 264:	00 00 00 00 	nop
 268:	97 e1 00 00 	lhu r1,0x0\(r31\)
 26c:	00 00 00 00 	nop
 270:	93 e1 00 00 	lbu r1,0x0\(r31\)
 274:	00 00 00 00 	nop
 278:	a3 e1 00 00 	sb r1,0x0\(r31\)
 27c:	00 00 00 00 	nop
 280:	a7 e1 00 00 	sh r1,0x0\(r31\)
 284:	00 00 00 00 	nop
 288:	af e1 00 00 	sw r1,0x0\(r31\)
 28c:	00 00 00 00 	nop
 290:	4c 21 f8 08 	rba r31,r1,r1
 294:	00 00 00 00 	nop
 298:	4c 21 f8 0a 	rbar r31,r1,r1
 29c:	00 00 00 00 	nop
 2a0:	4c 21 f8 09 	rbal r31,r1,r1
 2a4:	00 00 00 00 	nop
 2a8:	4c 21 f8 10 	wba r31,r1,r1
 2ac:	00 00 00 00 	nop
 2b0:	4c 21 f8 12 	wbac r31,r1,r1
 2b4:	00 00 00 00 	nop
 2b8:	4c 21 f8 11 	wbau r31,r1,r1
 2bc:	00 00 00 00 	nop
 2c0:	4c 21 fa 00 	rbi r31,r1,r1,0x0
 2c4:	00 00 00 00 	nop
 2c8:	4c 21 f9 00 	rbir r31,r1,r1,0x0
 2cc:	00 00 00 00 	nop
 2d0:	4c 21 fb 00 	rbil r31,r1,r1,0x0
 2d4:	00 00 00 00 	nop
 2d8:	4c 21 fe 00 	wbi r31,r1,r1,0x0
 2dc:	00 00 00 00 	nop
 2e0:	4c 21 fd 00 	wbic r31,r1,r1,0x0
 2e4:	00 00 00 00 	nop
 2e8:	4c 21 ff 00 	wbiu r31,r1,r1,0x0
 2ec:	00 00 00 00 	nop
 2f0:	4c 21 f8 28 	pkrla r31,r1,r1
 2f4:	00 00 00 00 	nop
 2f8:	4c 21 f8 2a 	pkrlah r31,r1,r1
 2fc:	00 00 00 00 	nop
 300:	4c 21 f8 29 	pkrlau r31,r1,r1
 304:	00 00 00 00 	nop
 308:	48 21 f8 00 	pkrli r31,r1,r1,0x0
 30c:	00 00 00 00 	nop
 310:	48 21 fa 00 	pkrlih r31,r1,r1,0x0
 314:	00 00 00 00 	nop
 318:	48 21 f9 00 	pkrliu r31,r1,r1,0x0
 31c:	00 00 00 00 	nop
 320:	4c 01 08 01 	lock r1,r1
 324:	00 00 00 00 	nop
 328:	4c 01 08 03 	unlk r1,r1
 32c:	00 00 00 00 	nop
 330:	4c 21 f8 06 	swwr r31,r1,r1
 334:	00 00 00 00 	nop
 338:	4c 21 f8 07 	swwru r31,r1,r1
 33c:	00 00 00 00 	nop
 340:	4c 01 08 04 	swrd r1,r1
 344:	00 00 00 00 	nop
 348:	4c 01 08 05 	swrdl r1,r1
 34c:	00 00 00 00 	nop
 350:	4c 02 10 0c 	dwrd r2,r2
 354:	00 00 00 00 	nop
 358:	4c 02 10 0d 	dwrdl r2,r2
 35c:	00 00 00 00 	nop
 360:	4c 1f 0c 08 	cam36 r1,r31,0x1,0x0
 364:	00 00 00 00 	nop
 368:	4c 1f 0c 41 	cam72 r1,r31,0x1,0x0
 36c:	00 00 00 00 	nop
 370:	4c 1f 0c 81 	cam144 r1,r31,0x1,0x0
 374:	00 00 00 00 	nop
 378:	4c 1f 0c c1 	cam288 r1,r31,0x1,0x0
 37c:	00 00 00 00 	nop
 380:	4f e1 08 ab 	cm32and r1,r31,r1
 384:	00 00 00 00 	nop
 388:	4f e1 08 a3 	cm32andn r1,r31,r1
 38c:	00 00 00 00 	nop
 390:	4f e1 08 aa 	cm32or r1,r31,r1
 394:	00 00 00 00 	nop
 398:	4f e1 08 b0 	cm32ra r1,r31,r1
 39c:	00 00 00 00 	nop
 3a0:	4c 01 08 a1 	cm32rd r1,r1
 3a4:	00 00 00 00 	nop
 3a8:	4c 01 08 a4 	cm32ri r1,r1
 3ac:	00 00 00 00 	nop
 3b0:	4f e1 08 a0 	cm32rs r1,r31,r1
 3b4:	00 00 00 00 	nop
 3b8:	4f e1 08 b8 	cm32sa r1,r31,r1
 3bc:	00 00 00 00 	nop
 3c0:	4c 01 08 a9 	cm32sd r1,r1
 3c4:	00 00 00 00 	nop
 3c8:	4c 01 08 ac 	cm32si r1,r1
 3cc:	00 00 00 00 	nop
 3d0:	4f e1 08 a8 	cm32ss r1,r31,r1
 3d4:	00 00 00 00 	nop
 3d8:	4f e1 08 a2 	cm32xor r1,r31,r1
 3dc:	00 00 00 00 	nop
 3e0:	4c 02 10 85 	cm64clr r2,r2
 3e4:	00 00 00 00 	nop
 3e8:	4f e2 10 90 	cm64ra r2,r31,r2
 3ec:	00 00 00 00 	nop
 3f0:	4c 02 10 81 	cm64rd r2,r2
 3f4:	00 00 00 00 	nop
 3f8:	4c 02 10 84 	cm64ri r2,r2
 3fc:	00 00 00 00 	nop
 400:	4f e2 10 94 	cm64ria2 r2,r31,r2
 404:	00 00 00 00 	nop
 408:	4f e2 10 80 	cm64rs r2,r31,r2
 40c:	00 00 00 00 	nop
 410:	4f e2 10 98 	cm64sa r2,r31,r2
 414:	00 00 00 00 	nop
 418:	4c 02 10 89 	cm64sd r2,r2
 41c:	00 00 00 00 	nop
 420:	4c 02 10 8c 	cm64si r2,r2
 424:	00 00 00 00 	nop
 428:	4f e2 10 9c 	cm64sia2 r2,r31,r2
 42c:	00 00 00 00 	nop
 430:	4f e2 10 88 	cm64ss r2,r31,r2
 434:	00 00 00 00 	nop
 438:	4f e2 10 95 	cm128ria2 r2,r31,r2
 43c:	00 00 00 00 	nop
 440:	4f e2 10 90 	cm64ra r2,r31,r2
 444:	00 00 00 00 	nop
 448:	4f e2 10 b1 	cm128ria4 r2,r31,r2,0x1
 44c:	00 00 00 00 	nop
 450:	4f e2 10 9d 	cm128sia2 r2,r31,r2
 454:	00 00 00 00 	nop
 458:	4f e2 10 98 	cm64sa r2,r31,r2
 45c:	00 00 00 00 	nop
 460:	4f e1 08 b8 	cm32sa r1,r31,r1
 464:	00 00 00 00 	nop
 468:	4f e1 08 a6 	cm128vsa r1,r31,r1
 46c:	00 00 00 00 	nop
 470:	4f e1 08 14 	crc32 r1,r31,r1
 474:	00 00 00 00 	nop
 478:	4f e1 08 15 	crc32b r1,r31,r1
 47c:	00 00 00 00 	nop
 480:	4c 20 08 26 	chkhdr r1,r1
 484:	00 00 00 00 	nop
 488:	4c 00 08 24 	avail r1
 48c:	00 00 00 00 	nop
 490:	4c 20 08 25 	free r1,r1
 494:	00 00 00 00 	nop
 498:	4f e0 08 27 	tstod r1,r31
 49c:	00 00 00 00 	nop
 4a0:	00 00 00 0e 	yield
 4a4:	00 00 00 00 	nop
 4a8:	4c 00 08 2c 	cmphdr r1
 4ac:	00 00 00 00 	nop
 4b0:	4c 01 08 20 	mcid r1,r1
 4b4:	00 00 00 00 	nop
 4b8:	4c 00 f8 22 	dba r31
 4bc:	00 00 00 00 	nop
 4c0:	4c 01 08 21 	dbd r1,r0,r1
 4c4:	00 00 00 00 	nop
 4c8:	4c 20 08 23 	dpwt r1,r1
 4cc:	00 00 00 00 	nop
