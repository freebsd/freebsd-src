#as:
#objdump: -dr
#name: allinsn

.*: +file format .*

Disassembly of section \.text:

00000000 <movlmemimm>:
   0:	00 78 00 00 	mov\.b 0x0,#0x0
   4:	ff 79 ff ff 	mov\.w 0xff,#0xffff
   8:	80 79 00 80 	mov\.w 0x80,#0x8000
   c:	7f 78 ff 7f 	mov\.b 0x7f,#0x7fff
  10:	01 79 01 00 	mov\.w 0x1,#0x1
  14:	51 79 a9 fc 	mov\.w 0x51,#0xfca9
  18:	f7 79 f6 a4 	mov\.w 0xf7,#0xa4f6
  1c:	54 78 07 41 	mov\.b 0x54,#0x4107

00000020 <movhmemimm>:
  20:	00 7a 00 00 	mov\.b 0x7f00,#0x0
  24:	ff 7b ff ff 	mov\.w 0x7fff,#0xffff
  28:	80 7b 00 80 	mov\.w 0x7f80,#0x8000
  2c:	7f 7a ff 7f 	mov\.b 0x7f7f,#0x7fff
  30:	01 7b 01 00 	mov\.w 0x7f01,#0x1
  34:	a5 7a 98 07 	mov\.b 0x7fa5,#0x798
  38:	ba 7b fd 2c 	mov\.w 0x7fba,#0x2cfd
  3c:	3f 7a d4 63 	mov\.b 0x7f3f,#0x63d4

00000040 <movlgrmem>:
  40:	00 80       	mov\.b r0,0x0
  42:	ff 8f       	mov\.w r7,0xff
  44:	80 89       	mov\.w r4,0x80
  46:	7f 86       	mov\.b r3,0x7f
  48:	01 83       	mov\.w r1,0x1
  4a:	b3 8d       	mov\.w r6,0xb3
  4c:	b7 81       	mov\.w r0,0xb7
  4e:	29 86       	mov\.b r3,0x29

00000050 <movhgrmem>:
  50:	00 a0       	mov\.b r0,0x7f00
  52:	ff af       	mov\.w r7,0x7fff
  54:	80 a9       	mov\.w r4,0x7f80
  56:	7f a6       	mov\.b r3,0x7f7f
  58:	01 a3       	mov\.w r1,0x7f01
  5a:	72 a4       	mov\.b r2,0x7f72
  5c:	d2 a5       	mov\.w r2,0x7fd2
  5e:	b5 ab       	mov\.w r5,0x7fb5

00000060 <movlmemgr>:
  60:	00 90       	mov\.b 0x0,r0
  62:	ff 9f       	mov\.w 0xff,r7
  64:	80 99       	mov\.w 0x80,r4
  66:	7f 96       	mov\.b 0x7f,r3
  68:	01 93       	mov\.w 0x1,r1
  6a:	89 91       	mov\.w 0x89,r0
  6c:	1a 91       	mov\.w 0x1a,r0
  6e:	7f 98       	mov\.b 0x7f,r4

00000070 <movhmemgr>:
  70:	00 b0       	mov\.b 0x7f00,r0
  72:	ff bf       	mov\.w 0x7fff,r7
  74:	80 b9       	mov\.w 0x7f80,r4
  76:	7f b6       	mov\.b 0x7f7f,r3
  78:	01 b3       	mov\.w 0x7f01,r1
  7a:	62 b7       	mov\.w 0x7f62,r3
  7c:	87 bf       	mov\.w 0x7f87,r7
  7e:	e5 b4       	mov\.b 0x7fe5,r2

00000080 <movgrgri>:
  80:	00 70       	mov\.b r0,\(r0\)
  82:	f7 71       	mov\.w r7,\(r15\)
  84:	84 71       	mov\.w r4,\(r8\)
  86:	73 70       	mov\.b r3,\(r7\)
  88:	11 71       	mov\.w r1,\(r1\)
  8a:	46 71       	mov\.w r6,\(r4\)
  8c:	c0 70       	mov\.b r0,\(r12\)
  8e:	95 71       	mov\.w r5,\(r9\)

00000090 <movgrgripostinc>:
  90:	00 60       	mov\.b r0,\(r0\+\+\)
  92:	f7 61       	mov\.w r7,\(r15\+\+\)
  94:	84 61       	mov\.w r4,\(r8\+\+\)
  96:	73 60       	mov\.b r3,\(r7\+\+\)
  98:	11 61       	mov\.w r1,\(r1\+\+\)
  9a:	84 61       	mov\.w r4,\(r8\+\+\)
  9c:	c3 61       	mov\.w r3,\(r12\+\+\)
  9e:	46 60       	mov\.b r6,\(r4\+\+\)

000000a0 <movgrgripredec>:
  a0:	00 68       	mov\.b r0,\(--r0\)
  a2:	f7 69       	mov\.w r7,\(--r15\)
  a4:	84 69       	mov\.w r4,\(--r8\)
  a6:	73 68       	mov\.b r3,\(--r7\)
  a8:	11 69       	mov\.w r1,\(--r1\)
  aa:	95 69       	mov\.w r5,\(--r9\)
  ac:	e4 69       	mov\.w r4,\(--r14\)
  ae:	74 68       	mov\.b r4,\(--r7\)

000000b0 <movgrigr>:
  b0:	00 72       	mov\.b \(r0\),r0
  b2:	f7 73       	mov\.w \(r15\),r7
  b4:	84 73       	mov\.w \(r8\),r4
  b6:	73 72       	mov\.b \(r7\),r3
  b8:	11 73       	mov\.w \(r1\),r1
  ba:	43 73       	mov\.w \(r4\),r3
  bc:	36 72       	mov\.b \(r3\),r6
  be:	70 73       	mov\.w \(r7\),r0

000000c0 <movgripostincgr>:
  c0:	00 62       	mov\.b \(r0\+\+\),r0
  c2:	f7 63       	mov\.w \(r15\+\+\),r7
  c4:	84 63       	mov\.w \(r8\+\+\),r4
  c6:	73 62       	mov\.b \(r7\+\+\),r3
  c8:	11 63       	mov\.w \(r1\+\+\),r1
  ca:	c5 63       	mov\.w \(r12\+\+\),r5
  cc:	42 62       	mov\.b \(r4\+\+\),r2
  ce:	b6 62       	mov\.b \(r11\+\+\),r6

000000d0 <movgripredecgr>:
  d0:	00 6a       	mov\.b \(--r0\),r0
  d2:	f7 6b       	mov\.w \(--r15\),r7
  d4:	84 6b       	mov\.w \(--r8\),r4
  d6:	73 6a       	mov\.b \(--r7\),r3
  d8:	11 6b       	mov\.w \(--r1\),r1
  da:	83 6a       	mov\.b \(--r8\),r3
  dc:	b4 6a       	mov\.b \(--r11\),r4
  de:	16 6b       	mov\.w \(--r1\),r6

000000e0 <movgrgrii>:
  e0:	08 70 00 00 	mov\.b r0,\(r0,0\)
  e4:	ff 71 ff 0f 	mov\.w r7,\(r15,-1\)
  e8:	8c 71 00 08 	mov\.w r4,\(r8,-2048\)
  ec:	7b 70 ff 07 	mov\.b r3,\(r7,2047\)
  f0:	19 71 01 00 	mov\.w r1,\(r1,1\)
  f4:	8e 71 3c 0e 	mov\.w r6,\(r8,-452\)
  f8:	bc 71 3c 02 	mov\.w r4,\(r11,572\)
  fc:	19 70 4a 09 	mov\.b r1,\(r1,-1718\)

00000100 <movgrgriipostinc>:
 100:	08 60 00 00 	mov\.b r0,\(r0\+\+,0\)
 104:	ff 61 ff 0f 	mov\.w r7,\(r15\+\+,-1\)
 108:	8c 61 00 08 	mov\.w r4,\(r8\+\+,-2048\)
 10c:	7b 60 ff 07 	mov\.b r3,\(r7\+\+,2047\)
 110:	19 61 01 00 	mov\.w r1,\(r1\+\+,1\)
 114:	0e 61 c0 0f 	mov\.w r6,\(r0\+\+,-64\)
 118:	ff 60 24 04 	mov\.b r7,\(r15\+\+,1060\)
 11c:	78 60 4f 03 	mov\.b r0,\(r7\+\+,847\)

00000120 <movgrgriipredec>:
 120:	08 68 00 00 	mov\.b r0,\(--r0,0\)
 124:	ff 69 ff 0f 	mov\.w r7,\(--r15,-1\)
 128:	8c 69 00 08 	mov\.w r4,\(--r8,-2048\)
 12c:	7b 68 ff 07 	mov\.b r3,\(--r7,2047\)
 130:	19 69 01 00 	mov\.w r1,\(--r1,1\)
 134:	f8 69 f4 06 	mov\.w r0,\(--r15,1780\)
 138:	1e 69 e2 05 	mov\.w r6,\(--r1,1506\)
 13c:	3f 69 0f 08 	mov\.w r7,\(--r3,-2033\)

00000140 <movgriigr>:
 140:	08 72 00 00 	mov\.b \(r0,0\),r0
 144:	ff 73 ff 0f 	mov\.w \(r15,-1\),r7
 148:	8c 73 00 08 	mov\.w \(r8,-2048\),r4
 14c:	7b 72 ff 07 	mov\.b \(r7,2047\),r3
 150:	19 73 01 00 	mov\.w \(r1,1\),r1
 154:	7d 73 9c 07 	mov\.w \(r7,1948\),r5
 158:	3c 72 b4 0c 	mov\.b \(r3,-844\),r4
 15c:	f8 73 a8 06 	mov\.w \(r15,1704\),r0

00000160 <movgriipostincgr>:
 160:	08 62 00 00 	mov\.b \(r0\+\+,0\),r0
 164:	ff 63 ff 0f 	mov\.w \(r15\+\+,-1\),r7
 168:	8c 63 00 08 	mov\.w \(r8\+\+,-2048\),r4
 16c:	7b 62 ff 07 	mov\.b \(r7\+\+,2047\),r3
 170:	19 63 01 00 	mov\.w \(r1\+\+,1\),r1
 174:	2f 63 50 0f 	mov\.w \(r2\+\+,-176\),r7
 178:	8c 63 6d 05 	mov\.w \(r8\+\+,1389\),r4
 17c:	38 62 2f 00 	mov\.b \(r3\+\+,47\),r0

00000180 <movgriipredecgr>:
 180:	08 6a 00 00 	mov\.b \(--r0,0\),r0
 184:	ff 6b ff 0f 	mov\.w \(--r15,-1\),r7
 188:	8c 6b 00 08 	mov\.w \(--r8,-2048\),r4
 18c:	7b 6a ff 07 	mov\.b \(--r7,2047\),r3
 190:	19 6b 01 00 	mov\.w \(--r1,1\),r1
 194:	8c 6a ec 03 	mov\.b \(--r8,1004\),r4
 198:	ea 6b 5c 0a 	mov\.w \(--r14,-1444\),r2
 19c:	5c 6a 61 0c 	mov\.b \(--r5,-927\),r4

000001a0 <movgrgr>:
 1a0:	00 46       	mov r0,r0
 1a2:	ff 46       	mov r15,r15
 1a4:	88 46       	mov r8,r8
 1a6:	77 46       	mov r7,r7
 1a8:	11 46       	mov r1,r1
 1aa:	e9 46       	mov r9,r14
 1ac:	f7 46       	mov r7,r15
 1ae:	fc 46       	mov r12,r15

000001b0 <movimm8>:
 1b0:	00 47       	mov Rx,#0x0
 1b2:	ff 47       	mov Rx,#0xff
 1b4:	80 47       	mov Rx,#0x80
 1b6:	7f 47       	mov Rx,#0x7f
 1b8:	01 47       	mov Rx,#0x1
 1ba:	88 47       	mov Rx,#0x88
 1bc:	53 47       	mov Rx,#0x53
 1be:	68 47       	mov Rx,#0x68

000001c0 <movwimm8>:
 1c0:	00 47       	mov Rx,#0x0
 1c2:	ff 47       	mov Rx,#0xff
 1c4:	80 47       	mov Rx,#0x80
 1c6:	7f 47       	mov Rx,#0x7f
 1c8:	01 47       	mov Rx,#0x1
 1ca:	5c 47       	mov Rx,#0x5c
 1cc:	61 47       	mov Rx,#0x61
 1ce:	04 47       	mov Rx,#0x4

000001d0 <movgrimm8>:
 1d0:	00 21       	mov r0,#0x0
 1d2:	ff 2f       	mov r7,#0xff
 1d4:	80 29       	mov r4,#0x80
 1d6:	7f 27       	mov r3,#0x7f
 1d8:	01 23       	mov r1,#0x1
 1da:	ce 25       	mov r2,#0xce
 1dc:	37 29       	mov r4,#0x37
 1de:	03 25       	mov r2,#0x3

000001e0 <movwgrimm8>:
 1e0:	00 21       	mov r0,#0x0
 1e2:	ff 2f       	mov r7,#0xff
 1e4:	80 29       	mov r4,#0x80
 1e6:	7f 27       	mov r3,#0x7f
 1e8:	01 23       	mov r1,#0x1
 1ea:	f3 29       	mov r4,#0xf3
 1ec:	37 27       	mov r3,#0x37
 1ee:	6c 25       	mov r2,#0x6c

000001f0 <movgrimm16>:
 1f0:	00 21       	mov r0,#0x0
 1f2:	3f 31 ff ff 	mov r15,#0xffff
 1f6:	38 31 00 80 	mov r8,#0x8000
 1fa:	37 31 ff 7f 	mov r7,#0x7fff
 1fe:	01 23       	mov r1,#0x1
 200:	34 31 62 4e 	mov r4,#0x4e62
 204:	33 31 16 1c 	mov r3,#0x1c16
 208:	32 31 64 3e 	mov r2,#0x3e64

0000020c <movwgrimm16>:
 20c:	00 21       	mov r0,#0x0
 20e:	3f 31 ff ff 	mov r15,#0xffff
 212:	38 31 00 80 	mov r8,#0x8000
 216:	37 31 ff 7f 	mov r7,#0x7fff
 21a:	01 23       	mov r1,#0x1
 21c:	36 31 08 41 	mov r6,#0x4108
 220:	38 31 f1 68 	mov r8,#0x68f1
 224:	3a 31 2a 4e 	mov r10,#0x4e2a

00000228 <movlowgr>:
 228:	c0 30       	mov\.b r0,RxL
 22a:	cf 30       	mov\.b r15,RxL
 22c:	c8 30       	mov\.b r8,RxL
 22e:	c7 30       	mov\.b r7,RxL
 230:	c1 30       	mov\.b r1,RxL
 232:	cb 30       	mov\.b r11,RxL
 234:	c5 30       	mov\.b r5,RxL
 236:	c2 30       	mov\.b r2,RxL

00000238 <movhighgr>:
 238:	d0 30       	mov\.b r0,RxH
 23a:	df 30       	mov\.b r15,RxH
 23c:	d8 30       	mov\.b r8,RxH
 23e:	d7 30       	mov\.b r7,RxH
 240:	d1 30       	mov\.b r1,RxH
 242:	d2 30       	mov\.b r2,RxH
 244:	d7 30       	mov\.b r7,RxH
 246:	d2 30       	mov\.b r2,RxH

00000248 <movfgrgri>:
 248:	00 74       	movf\.b r0,\(r0\)
 24a:	f7 75       	movf\.w r7,\(r15\)
 24c:	84 75       	movf\.w r4,\(r8\)
 24e:	73 74       	movf\.b r3,\(r7\)
 250:	11 75       	movf\.w r1,\(r1\)
 252:	f6 74       	movf\.b r6,\(r15\)
 254:	a1 74       	movf\.b r1,\(r10\)
 256:	16 74       	movf\.b r6,\(r1\)

00000258 <movfgrgripostinc>:
 258:	00 64       	movf\.b r0,\(r0\+\+\)
 25a:	f7 65       	movf\.w r7,\(r15\+\+\)
 25c:	84 65       	movf\.w r4,\(r8\+\+\)
 25e:	73 64       	movf\.b r3,\(r7\+\+\)
 260:	11 65       	movf\.w r1,\(r1\+\+\)
 262:	52 64       	movf\.b r2,\(r5\+\+\)
 264:	a5 65       	movf\.w r5,\(r10\+\+\)
 266:	57 65       	movf\.w r7,\(r5\+\+\)

00000268 <movfgrgripredec>:
 268:	00 6c       	movf\.b r0,\(--r0\)
 26a:	f7 6d       	movf\.w r7,\(--r15\)
 26c:	84 6d       	movf\.w r4,\(--r8\)
 26e:	73 6c       	movf\.b r3,\(--r7\)
 270:	11 6d       	movf\.w r1,\(--r1\)
 272:	a6 6d       	movf\.w r6,\(--r10\)
 274:	e1 6c       	movf\.b r1,\(--r14\)
 276:	73 6d       	movf\.w r3,\(--r7\)

00000278 <movfgrigr>:
 278:	00 76       	movf\.b \(r0\),r0
 27a:	f7 77       	movf\.w \(r15\),r7
 27c:	84 77       	movf\.w \(r8\),r4
 27e:	73 76       	movf\.b \(r7\),r3
 280:	11 77       	movf\.w \(r1\),r1
 282:	54 76       	movf\.b \(r5\),r4
 284:	34 76       	movf\.b \(r3\),r4
 286:	c3 77       	movf\.w \(r12\),r3

00000288 <movfgripostincgr>:
 288:	00 66       	movf\.b \(r0\+\+\),r0
 28a:	f7 67       	movf\.w \(r15\+\+\),r7
 28c:	84 67       	movf\.w \(r8\+\+\),r4
 28e:	73 66       	movf\.b \(r7\+\+\),r3
 290:	11 67       	movf\.w \(r1\+\+\),r1
 292:	95 66       	movf\.b \(r9\+\+\),r5
 294:	a4 67       	movf\.w \(r10\+\+\),r4
 296:	91 66       	movf\.b \(r9\+\+\),r1

00000298 <movfgripredecgr>:
 298:	00 6e       	movf\.b \(--r0\),r0
 29a:	f7 6f       	movf\.w \(--r15\),r7
 29c:	84 6f       	movf\.w \(--r8\),r4
 29e:	73 6e       	movf\.b \(--r7\),r3
 2a0:	11 6f       	movf\.w \(--r1\),r1
 2a2:	02 6e       	movf\.b \(--r0\),r2
 2a4:	b2 6f       	movf\.w \(--r11\),r2
 2a6:	a5 6e       	movf\.b \(--r10\),r5

000002a8 <movfgrgrii>:
 2a8:	08 74 00 00 	movf\.b r0,\(r8,r0,0\)
 2ac:	ff 75 ff 7f 	movf\.w r7,\(r15,r15,-1\)
 2b0:	8c 75 00 48 	movf\.w r4,\(r12,r8,-2048\)
 2b4:	7b 74 ff 37 	movf\.b r3,\(r11,r7,2047\)
 2b8:	19 75 01 10 	movf\.w r1,\(r9,r1,1\)
 2bc:	0f 74 c1 75 	movf\.b r7,\(r15,r0,1473\)
 2c0:	9a 75 0e 0a 	movf\.w r2,\(r8,r9,-1522\)
 2c4:	1a 75 e0 51 	movf\.w r2,\(r13,r1,480\)

000002c8 <movfgrgriipostinc>:
 2c8:	08 64 00 00 	movf\.b r0,\(r8,r0\+\+,0\)
 2cc:	ff 65 ff 7f 	movf\.w r7,\(r15,r15\+\+,-1\)
 2d0:	8c 65 00 48 	movf\.w r4,\(r12,r8\+\+,-2048\)
 2d4:	7b 64 ff 37 	movf\.b r3,\(r11,r7\+\+,2047\)
 2d8:	19 65 01 10 	movf\.w r1,\(r9,r1\+\+,1\)
 2dc:	29 64 76 05 	movf\.b r1,\(r8,r2\+\+,1398\)
 2e0:	9c 65 f6 0c 	movf\.w r4,\(r8,r9\+\+,-778\)
 2e4:	e9 65 1c 56 	movf\.w r1,\(r13,r14\+\+,1564\)

000002e8 <movfgrgriipredec>:
 2e8:	08 6c 00 00 	movf\.b r0,\(r8,--r0,0\)
 2ec:	ff 6d ff 7f 	movf\.w r7,\(r15,--r15,-1\)
 2f0:	8c 6d 00 48 	movf\.w r4,\(r12,--r8,-2048\)
 2f4:	7b 6c ff 37 	movf\.b r3,\(r11,--r7,2047\)
 2f8:	19 6d 01 10 	movf\.w r1,\(r9,--r1,1\)
 2fc:	7e 6c fe 00 	movf\.b r6,\(r8,--r7,254\)
 300:	cd 6d 89 46 	movf\.w r5,\(r12,--r12,1673\)
 304:	a8 6c da 0f 	movf\.b r0,\(r8,--r10,-38\)

00000308 <movfgriigr>:
 308:	08 76 00 00 	movf\.b \(r8,r0,0\),r0
 30c:	ff 77 ff 7f 	movf\.w \(r15,r15,-1\),r7
 310:	8c 77 00 48 	movf\.w \(r12,r8,-2048\),r4
 314:	7b 76 ff 37 	movf\.b \(r11,r7,2047\),r3
 318:	19 77 01 10 	movf\.w \(r9,r1,1\),r1
 31c:	2b 77 9c 79 	movf\.w \(r15,r2,-1636\),r3
 320:	c9 77 5a 66 	movf\.w \(r14,r12,1626\),r1
 324:	e8 76 04 36 	movf\.b \(r11,r14,1540\),r0

00000328 <movfgriipostincgr>:
 328:	08 66 00 00 	movf\.b \(r8,r0\+\+,0\),r0
 32c:	ff 67 ff 7f 	movf\.w \(r15,r15\+\+,-1\),r7
 330:	8c 67 00 48 	movf\.w \(r12,r8\+\+,-2048\),r4
 334:	7b 66 ff 37 	movf\.b \(r11,r7\+\+,2047\),r3
 338:	19 67 01 10 	movf\.w \(r9,r1\+\+,1\),r1
 33c:	db 66 d2 71 	movf\.b \(r15,r13\+\+,466\),r3
 340:	bc 66 fa 30 	movf\.b \(r11,r11\+\+,250\),r4
 344:	af 66 38 2a 	movf\.b \(r10,r10\+\+,-1480\),r7

00000348 <movfgriipredecgr>:
 348:	08 6e 00 00 	movf\.b \(r8,--r0,0\),r0
 34c:	ff 6f ff 7f 	movf\.w \(r15,--r15,-1\),r7
 350:	8c 6f 00 48 	movf\.w \(r12,--r8,-2048\),r4
 354:	7b 6e ff 37 	movf\.b \(r11,--r7,2047\),r3
 358:	19 6f 01 10 	movf\.w \(r9,--r1,1\),r1
 35c:	a8 6e a0 5d 	movf\.b \(r13,--r10,-608\),r0
 360:	bf 6e 3f 13 	movf\.b \(r9,--r11,831\),r7
 364:	fe 6f 0c 78 	movf\.w \(r15,--r15,-2036\),r6

00000368 <maskgrgr>:
 368:	00 33       	mask r0,r0
 36a:	ff 33       	mask r15,r15
 36c:	88 33       	mask r8,r8
 36e:	77 33       	mask r7,r7
 370:	11 33       	mask r1,r1
 372:	04 33       	mask r4,r0
 374:	b6 33       	mask r6,r11
 376:	48 33       	mask r8,r4

00000378 <maskgrimm16>:
 378:	e0 30 00 00 	mask r0,#0x0
 37c:	ef 30 ff ff 	mask r15,#0xffff
 380:	e8 30 00 80 	mask r8,#0x8000
 384:	e7 30 ff 7f 	mask r7,#0x7fff
 388:	e1 30 01 00 	mask r1,#0x1
 38c:	e7 30 e9 46 	mask r7,#0x46e9
 390:	ef 30 64 1d 	mask r15,#0x1d64
 394:	ee 30 2d 86 	mask r14,#0x862d

00000398 <pushgr>:
 398:	80 00       	push r0
 39a:	8f 00       	push r15
 39c:	88 00       	push r8
 39e:	87 00       	push r7
 3a0:	81 00       	push r1
 3a2:	89 00       	push r9
 3a4:	84 00       	push r4
 3a6:	83 00       	push r3

000003a8 <popgr>:
 3a8:	90 00       	pop r0
 3aa:	9f 00       	pop r15
 3ac:	98 00       	pop r8
 3ae:	97 00       	pop r7
 3b0:	91 00       	pop r1
 3b2:	93 00       	pop r3
 3b4:	92 00       	pop r2
 3b6:	9c 00       	pop r12

000003b8 <swpn>:
 3b8:	90 30       	swpn r0
 3ba:	9f 30       	swpn r15
 3bc:	98 30       	swpn r8
 3be:	97 30       	swpn r7
 3c0:	91 30       	swpn r1
 3c2:	9f 30       	swpn r15
 3c4:	94 30       	swpn r4
 3c6:	93 30       	swpn r3

000003c8 <swpb>:
 3c8:	80 30       	swpb r0
 3ca:	8f 30       	swpb r15
 3cc:	88 30       	swpb r8
 3ce:	87 30       	swpb r7
 3d0:	81 30       	swpb r1
 3d2:	82 30       	swpb r2
 3d4:	8c 30       	swpb r12
 3d6:	82 30       	swpb r2

000003d8 <swpw>:
 3d8:	00 32       	swpw r0,r0
 3da:	ff 32       	swpw r15,r15
 3dc:	88 32       	swpw r8,r8
 3de:	77 32       	swpw r7,r7
 3e0:	11 32       	swpw r1,r1
 3e2:	4c 32       	swpw r12,r4
 3e4:	28 32       	swpw r8,r2
 3e6:	d5 32       	swpw r5,r13

000003e8 <andgrgr>:
 3e8:	00 40       	and r0,r0
 3ea:	ff 40       	and r15,r15
 3ec:	88 40       	and r8,r8
 3ee:	77 40       	and r7,r7
 3f0:	11 40       	and r1,r1
 3f2:	22 40       	and r2,r2
 3f4:	5f 40       	and r15,r5
 3f6:	57 40       	and r7,r5

000003f8 <andimm8>:
 3f8:	00 41       	and Rx,#0x0
 3fa:	ff 41       	and Rx,#0xff
 3fc:	80 41       	and Rx,#0x80
 3fe:	7f 41       	and Rx,#0x7f
 400:	01 41       	and Rx,#0x1
 402:	ce 41       	and Rx,#0xce
 404:	0b 41       	and Rx,#0xb
 406:	e8 41       	and Rx,#0xe8

00000408 <andgrimm16>:
 408:	00 31 00 00 	and r0,#0x0
 40c:	0f 31 ff ff 	and r15,#0xffff
 410:	08 31 00 80 	and r8,#0x8000
 414:	07 31 ff 7f 	and r7,#0x7fff
 418:	01 31 01 00 	and r1,#0x1
 41c:	0a 31 4d 43 	and r10,#0x434d
 420:	0b 31 0b f0 	and r11,#0xf00b
 424:	05 31 4d b7 	and r5,#0xb74d

00000428 <orgrgr>:
 428:	00 42       	or r0,r0
 42a:	ff 42       	or r15,r15
 42c:	88 42       	or r8,r8
 42e:	77 42       	or r7,r7
 430:	11 42       	or r1,r1
 432:	53 42       	or r3,r5
 434:	fe 42       	or r14,r15
 436:	c5 42       	or r5,r12

00000438 <orimm8>:
 438:	00 43       	or Rx,#0x0
 43a:	ff 43       	or Rx,#0xff
 43c:	80 43       	or Rx,#0x80
 43e:	7f 43       	or Rx,#0x7f
 440:	01 43       	or Rx,#0x1
 442:	04 43       	or Rx,#0x4
 444:	26 43       	or Rx,#0x26
 446:	34 43       	or Rx,#0x34

00000448 <orgrimm16>:
 448:	10 31 00 00 	or r0,#0x0
 44c:	1f 31 ff ff 	or r15,#0xffff
 450:	18 31 00 80 	or r8,#0x8000
 454:	17 31 ff 7f 	or r7,#0x7fff
 458:	11 31 01 00 	or r1,#0x1
 45c:	12 31 33 fc 	or r2,#0xfc33
 460:	12 31 db 47 	or r2,#0x47db
 464:	11 31 53 f6 	or r1,#0xf653

00000468 <xorgrgr>:
 468:	00 44       	xor r0,r0
 46a:	ff 44       	xor r15,r15
 46c:	88 44       	xor r8,r8
 46e:	77 44       	xor r7,r7
 470:	11 44       	xor r1,r1
 472:	1e 44       	xor r14,r1
 474:	99 44       	xor r9,r9
 476:	8c 44       	xor r12,r8

00000478 <xorimm8>:
 478:	00 45       	xor Rx,#0x0
 47a:	ff 45       	xor Rx,#0xff
 47c:	80 45       	xor Rx,#0x80
 47e:	7f 45       	xor Rx,#0x7f
 480:	01 45       	xor Rx,#0x1
 482:	d0 45       	xor Rx,#0xd0
 484:	7e 45       	xor Rx,#0x7e
 486:	37 45       	xor Rx,#0x37

00000488 <xorgrimm16>:
 488:	20 31 00 00 	xor r0,#0x0
 48c:	2f 31 ff ff 	xor r15,#0xffff
 490:	28 31 00 80 	xor r8,#0x8000
 494:	27 31 ff 7f 	xor r7,#0x7fff
 498:	21 31 01 00 	xor r1,#0x1
 49c:	2f 31 75 dc 	xor r15,#0xdc75
 4a0:	23 31 85 03 	xor r3,#0x385
 4a4:	22 31 99 90 	xor r2,#0x9099

000004a8 <notgr>:
 4a8:	b0 30       	not r0
 4aa:	bf 30       	not r15
 4ac:	b8 30       	not r8
 4ae:	b7 30       	not r7
 4b0:	b1 30       	not r1
 4b2:	b4 30       	not r4
 4b4:	b3 30       	not r3
 4b6:	b3 30       	not r3

000004b8 <addgrgr>:
 4b8:	00 49       	add r0,r0
 4ba:	ff 49       	add r15,r15
 4bc:	88 49       	add r8,r8
 4be:	77 49       	add r7,r7
 4c0:	11 49       	add r1,r1
 4c2:	7c 49       	add r12,r7
 4c4:	a1 49       	add r1,r10
 4c6:	ee 49       	add r14,r14

000004c8 <addgrimm4>:
 4c8:	00 51       	add r0,#0x0
 4ca:	ff 51       	add r15,#0xf
 4cc:	88 51       	add r8,#0x8
 4ce:	77 51       	add r7,#0x7
 4d0:	11 51       	add r1,#0x1
 4d2:	07 51       	add r7,#0x0
 4d4:	9a 51       	add r10,#0x9
 4d6:	87 51       	add r7,#0x8

000004d8 <addimm8>:
 4d8:	00 59       	add Rx,#0x0
 4da:	ff 59       	add Rx,#0xff
 4dc:	80 59       	add Rx,#0x80
 4de:	7f 59       	add Rx,#0x7f
 4e0:	01 59       	add Rx,#0x1
 4e2:	19 59       	add Rx,#0x19
 4e4:	f7 59       	add Rx,#0xf7
 4e6:	dd 59       	add Rx,#0xdd

000004e8 <addgrimm16>:
 4e8:	00 51       	add r0,#0x0
 4ea:	4f 31 ff 00 	add r15,#0xff
 4ee:	48 31 80 00 	add r8,#0x80
 4f2:	47 31 7f 00 	add r7,#0x7f
 4f6:	11 51       	add r1,#0x1
 4f8:	43 31 63 00 	add r3,#0x63
 4fc:	f0 51       	add r0,#0xf
 4fe:	47 31 d6 00 	add r7,#0xd6

00000502 <adcgrgr>:
 502:	00 4b       	adc r0,r0
 504:	ff 4b       	adc r15,r15
 506:	88 4b       	adc r8,r8
 508:	77 4b       	adc r7,r7
 50a:	11 4b       	adc r1,r1
 50c:	d2 4b       	adc r2,r13
 50e:	ae 4b       	adc r14,r10
 510:	f2 4b       	adc r2,r15

00000512 <adcgrimm4>:
 512:	00 53       	adc r0,#0x0
 514:	ff 53       	adc r15,#0xf
 516:	88 53       	adc r8,#0x8
 518:	77 53       	adc r7,#0x7
 51a:	11 53       	adc r1,#0x1
 51c:	1f 53       	adc r15,#0x1
 51e:	31 53       	adc r1,#0x3
 520:	b6 53       	adc r6,#0xb

00000522 <adcimm8>:
 522:	00 5b       	adc Rx,#0x0
 524:	ff 5b       	adc Rx,#0xff
 526:	80 5b       	adc Rx,#0x80
 528:	7f 5b       	adc Rx,#0x7f
 52a:	01 5b       	adc Rx,#0x1
 52c:	e1 5b       	adc Rx,#0xe1
 52e:	4b 5b       	adc Rx,#0x4b
 530:	12 5b       	adc Rx,#0x12

00000532 <adcgrimm16>:
 532:	00 53       	adc r0,#0x0
 534:	5f 31 ff ff 	adc r15,#0xffff
 538:	58 31 00 80 	adc r8,#0x8000
 53c:	57 31 ff 7f 	adc r7,#0x7fff
 540:	11 53       	adc r1,#0x1
 542:	5d 31 99 f6 	adc r13,#0xf699
 546:	53 31 f3 5c 	adc r3,#0x5cf3
 54a:	5b 31 5d c0 	adc r11,#0xc05d

0000054e <subgrgr>:
 54e:	00 4d       	sub r0,r0
 550:	ff 4d       	sub r15,r15
 552:	88 4d       	sub r8,r8
 554:	77 4d       	sub r7,r7
 556:	11 4d       	sub r1,r1
 558:	88 4d       	sub r8,r8
 55a:	99 4d       	sub r9,r9
 55c:	f9 4d       	sub r9,r15

0000055e <subgrimm4>:
 55e:	00 55       	sub r0,#0x0
 560:	ff 55       	sub r15,#0xf
 562:	88 55       	sub r8,#0x8
 564:	77 55       	sub r7,#0x7
 566:	11 55       	sub r1,#0x1
 568:	f2 55       	sub r2,#0xf
 56a:	9c 55       	sub r12,#0x9
 56c:	48 55       	sub r8,#0x4

0000056e <subimm8>:
 56e:	00 5d       	sub Rx,#0x0
 570:	ff 5d       	sub Rx,#0xff
 572:	80 5d       	sub Rx,#0x80
 574:	7f 5d       	sub Rx,#0x7f
 576:	01 5d       	sub Rx,#0x1
 578:	cd 5d       	sub Rx,#0xcd
 57a:	99 5d       	sub Rx,#0x99
 57c:	d9 5d       	sub Rx,#0xd9

0000057e <subgrimm16>:
 57e:	00 55       	sub r0,#0x0
 580:	6f 31 ff ff 	sub r15,#0xffff
 584:	68 31 00 80 	sub r8,#0x8000
 588:	67 31 ff 7f 	sub r7,#0x7fff
 58c:	11 55       	sub r1,#0x1
 58e:	63 31 b7 ca 	sub r3,#0xcab7
 592:	6b 31 41 5c 	sub r11,#0x5c41
 596:	6a 31 4a 1e 	sub r10,#0x1e4a

0000059a <sbcgrgr>:
 59a:	00 4f       	sbc r0,r0
 59c:	ff 4f       	sbc r15,r15
 59e:	88 4f       	sbc r8,r8
 5a0:	77 4f       	sbc r7,r7
 5a2:	11 4f       	sbc r1,r1
 5a4:	2b 4f       	sbc r11,r2
 5a6:	19 4f       	sbc r9,r1
 5a8:	f4 4f       	sbc r4,r15

000005aa <sbcgrimm4>:
 5aa:	00 57       	sbc r0,#0x0
 5ac:	ff 57       	sbc r15,#0xf
 5ae:	88 57       	sbc r8,#0x8
 5b0:	77 57       	sbc r7,#0x7
 5b2:	11 57       	sbc r1,#0x1
 5b4:	ba 57       	sbc r10,#0xb
 5b6:	ab 57       	sbc r11,#0xa
 5b8:	ad 57       	sbc r13,#0xa

000005ba <sbcgrimm8>:
 5ba:	00 5f       	sbc Rx,#0x0
 5bc:	ff 5f       	sbc Rx,#0xff
 5be:	80 5f       	sbc Rx,#0x80
 5c0:	7f 5f       	sbc Rx,#0x7f
 5c2:	01 5f       	sbc Rx,#0x1
 5c4:	89 5f       	sbc Rx,#0x89
 5c6:	e0 5f       	sbc Rx,#0xe0
 5c8:	9c 5f       	sbc Rx,#0x9c

000005ca <sbcgrimm16>:
 5ca:	00 57       	sbc r0,#0x0
 5cc:	7f 31 ff ff 	sbc r15,#0xffff
 5d0:	78 31 00 80 	sbc r8,#0x8000
 5d4:	77 31 ff 7f 	sbc r7,#0x7fff
 5d8:	11 57       	sbc r1,#0x1
 5da:	70 31 fb 7e 	sbc r0,#0x7efb
 5de:	77 31 a2 21 	sbc r7,#0x21a2
 5e2:	7e 31 95 4f 	sbc r14,#0x4f95

000005e6 <incgr>:
 5e6:	00 30       	inc r0
 5e8:	0f 30       	inc r15
 5ea:	08 30       	inc r8
 5ec:	07 30       	inc r7
 5ee:	01 30       	inc r1
 5f0:	0d 30       	inc r13
 5f2:	01 30       	inc r1
 5f4:	0b 30       	inc r11

000005f6 <incgrimm2>:
 5f6:	00 30       	inc r0
 5f8:	3f 30       	inc r15,#0x3
 5fa:	28 30       	inc r8,#0x2
 5fc:	17 30       	inc r7,#0x1
 5fe:	11 30       	inc r1,#0x1
 600:	1e 30       	inc r14,#0x1
 602:	05 30       	inc r5
 604:	3c 30       	inc r12,#0x3

00000606 <decgr>:
 606:	40 30       	dec r0
 608:	4f 30       	dec r15
 60a:	48 30       	dec r8
 60c:	47 30       	dec r7
 60e:	41 30       	dec r1
 610:	4c 30       	dec r12
 612:	48 30       	dec r8
 614:	4a 30       	dec r10

00000616 <decgrimm2>:
 616:	40 30       	dec r0
 618:	7f 30       	dec r15,#0x3
 61a:	68 30       	dec r8,#0x2
 61c:	57 30       	dec r7,#0x1
 61e:	51 30       	dec r1,#0x1
 620:	45 30       	dec r5
 622:	4d 30       	dec r13
 624:	6d 30       	dec r13,#0x2

00000626 <rrcgrgr>:
 626:	00 38       	rrc r0,r0
 628:	ff 38       	rrc r15,r15
 62a:	88 38       	rrc r8,r8
 62c:	77 38       	rrc r7,r7
 62e:	11 38       	rrc r1,r1
 630:	48 38       	rrc r8,r4
 632:	ea 38       	rrc r10,r14
 634:	9f 38       	rrc r15,r9

00000636 <rrcgrimm4>:
 636:	00 39       	rrc r0,#0x0
 638:	ff 39       	rrc r15,#0xf
 63a:	88 39       	rrc r8,#0x8
 63c:	77 39       	rrc r7,#0x7
 63e:	11 39       	rrc r1,#0x1
 640:	3b 39       	rrc r11,#0x3
 642:	ce 39       	rrc r14,#0xc
 644:	f2 39       	rrc r2,#0xf

00000646 <rlcgrgr>:
 646:	00 3a       	rlc r0,r0
 648:	ff 3a       	rlc r15,r15
 64a:	88 3a       	rlc r8,r8
 64c:	77 3a       	rlc r7,r7
 64e:	11 3a       	rlc r1,r1
 650:	3f 3a       	rlc r15,r3
 652:	7f 3a       	rlc r15,r7
 654:	af 3a       	rlc r15,r10

00000656 <rlcgrimm4>:
 656:	00 3b       	rlc r0,#0x0
 658:	ff 3b       	rlc r15,#0xf
 65a:	88 3b       	rlc r8,#0x8
 65c:	77 3b       	rlc r7,#0x7
 65e:	11 3b       	rlc r1,#0x1
 660:	28 3b       	rlc r8,#0x2
 662:	62 3b       	rlc r2,#0x6
 664:	a6 3b       	rlc r6,#0xa

00000666 <shrgrgr>:
 666:	00 3c       	shr r0,r0
 668:	ff 3c       	shr r15,r15
 66a:	88 3c       	shr r8,r8
 66c:	77 3c       	shr r7,r7
 66e:	11 3c       	shr r1,r1
 670:	2d 3c       	shr r13,r2
 672:	87 3c       	shr r7,r8
 674:	86 3c       	shr r6,r8

00000676 <shrgrimm>:
 676:	00 3d       	shr r0,#0x0
 678:	ff 3d       	shr r15,#0xf
 67a:	88 3d       	shr r8,#0x8
 67c:	77 3d       	shr r7,#0x7
 67e:	11 3d       	shr r1,#0x1
 680:	d9 3d       	shr r9,#0xd
 682:	72 3d       	shr r2,#0x7
 684:	88 3d       	shr r8,#0x8

00000686 <shlgrgr>:
 686:	00 3e       	shl r0,r0
 688:	ff 3e       	shl r15,r15
 68a:	88 3e       	shl r8,r8
 68c:	77 3e       	shl r7,r7
 68e:	11 3e       	shl r1,r1
 690:	32 3e       	shl r2,r3
 692:	30 3e       	shl r0,r3
 694:	12 3e       	shl r2,r1

00000696 <shlgrimm>:
 696:	00 3f       	shl r0,#0x0
 698:	ff 3f       	shl r15,#0xf
 69a:	88 3f       	shl r8,#0x8
 69c:	77 3f       	shl r7,#0x7
 69e:	11 3f       	shl r1,#0x1
 6a0:	d6 3f       	shl r6,#0xd
 6a2:	63 3f       	shl r3,#0x6
 6a4:	ff 3f       	shl r15,#0xf

000006a6 <asrgrgr>:
 6a6:	00 36       	asr r0,r0
 6a8:	ff 36       	asr r15,r15
 6aa:	88 36       	asr r8,r8
 6ac:	77 36       	asr r7,r7
 6ae:	11 36       	asr r1,r1
 6b0:	a5 36       	asr r5,r10
 6b2:	53 36       	asr r3,r5
 6b4:	b6 36       	asr r6,r11

000006b6 <asrgrimm>:
 6b6:	00 37       	asr r0,#0x0
 6b8:	ff 37       	asr r15,#0xf
 6ba:	88 37       	asr r8,#0x8
 6bc:	77 37       	asr r7,#0x7
 6be:	11 37       	asr r1,#0x1
 6c0:	4d 37       	asr r13,#0x4
 6c2:	d0 37       	asr r0,#0xd
 6c4:	36 37       	asr r6,#0x3

000006c6 <set1grimm>:
 6c6:	00 09       	set1 r0,#0x0
 6c8:	ff 09       	set1 r15,#0xf
 6ca:	88 09       	set1 r8,#0x8
 6cc:	77 09       	set1 r7,#0x7
 6ce:	11 09       	set1 r1,#0x1
 6d0:	a6 09       	set1 r6,#0xa
 6d2:	1d 09       	set1 r13,#0x1
 6d4:	fd 09       	set1 r13,#0xf

000006d6 <set1grgr>:
 6d6:	00 0b       	set1 r0,r0
 6d8:	ff 0b       	set1 r15,r15
 6da:	88 0b       	set1 r8,r8
 6dc:	77 0b       	set1 r7,r7
 6de:	11 0b       	set1 r1,r1
 6e0:	06 0b       	set1 r6,r0
 6e2:	76 0b       	set1 r6,r7
 6e4:	2e 0b       	set1 r14,r2

000006e6 <set1lmemimm>:
 6e6:	00 e1       	set1 0x0,#0x0
 6e8:	ff ef       	set1 0xff,#0x7
 6ea:	80 e9       	set1 0x80,#0x4
 6ec:	7f e7       	set1 0x7f,#0x3
 6ee:	01 e3       	set1 0x1,#0x1
 6f0:	f4 e7       	set1 0xf4,#0x3
 6f2:	37 ef       	set1 0x37,#0x7
 6f4:	fc eb       	set1 0xfc,#0x5

000006f6 <set1hmemimm>:
 6f6:	00 f1       	set1 0x7f00,#0x0
 6f8:	ff ff       	set1 0x7fff,#0x7
 6fa:	80 f9       	set1 0x7f80,#0x4
 6fc:	7f f7       	set1 0x7f7f,#0x3
 6fe:	01 f3       	set1 0x7f01,#0x1
 700:	0a f7       	set1 0x7f0a,#0x3
 702:	63 f9       	set1 0x7f63,#0x4
 704:	94 f7       	set1 0x7f94,#0x3

00000706 <clr1grimm>:
 706:	00 08       	clr1 r0,#0x0
 708:	ff 08       	clr1 r15,#0xf
 70a:	88 08       	clr1 r8,#0x8
 70c:	77 08       	clr1 r7,#0x7
 70e:	11 08       	clr1 r1,#0x1
 710:	0c 08       	clr1 r12,#0x0
 712:	b8 08       	clr1 r8,#0xb
 714:	77 08       	clr1 r7,#0x7

00000716 <clr1grgr>:
 716:	00 0a       	clr1 r0,r0
 718:	ff 0a       	clr1 r15,r15
 71a:	88 0a       	clr1 r8,r8
 71c:	77 0a       	clr1 r7,r7
 71e:	11 0a       	clr1 r1,r1
 720:	33 0a       	clr1 r3,r3
 722:	10 0a       	clr1 r0,r1
 724:	0f 0a       	clr1 r15,r0

00000726 <clr1lmemimm>:
 726:	00 e0       	clr1 0x0,#0x0
 728:	ff ee       	clr1 0xff,#0x7
 72a:	80 e8       	clr1 0x80,#0x4
 72c:	7f e6       	clr1 0x7f,#0x3
 72e:	01 e2       	clr1 0x1,#0x1
 730:	72 ee       	clr1 0x72,#0x7
 732:	e5 e8       	clr1 0xe5,#0x4
 734:	56 e2       	clr1 0x56,#0x1

00000736 <clr1hmemimm>:
 736:	00 f0       	clr1 0x7f00,#0x0
 738:	ff fe       	clr1 0x7fff,#0x7
 73a:	80 f8       	clr1 0x7f80,#0x4
 73c:	7f f6       	clr1 0x7f7f,#0x3
 73e:	01 f2       	clr1 0x7f01,#0x1
 740:	2c f6       	clr1 0x7f2c,#0x3
 742:	d4 fa       	clr1 0x7fd4,#0x5
 744:	43 fe       	clr1 0x7f43,#0x7

00000746 <cbwgr>:
 746:	a0 30       	cbw r0
 748:	af 30       	cbw r15
 74a:	a8 30       	cbw r8
 74c:	a7 30       	cbw r7
 74e:	a1 30       	cbw r1
 750:	a8 30       	cbw r8
 752:	ab 30       	cbw r11
 754:	a3 30       	cbw r3

00000756 <revgr>:
 756:	f0 30       	rev r0
 758:	ff 30       	rev r15
 75a:	f8 30       	rev r8
 75c:	f7 30       	rev r7
 75e:	f1 30       	rev r1
 760:	f1 30       	rev r1
 762:	f1 30       	rev r1
 764:	fe 30       	rev r14

00000766 <bgr>:
 766:	20 00       	br r0
 768:	2f 00       	br r15
 76a:	28 00       	br r8
 76c:	27 00       	br r7
 76e:	21 00       	br r1
 770:	20 00       	br r0
 772:	2f 00       	br r15
 774:	2c 00       	br r12

00000776 <jmp>:
 776:	40 00       	jmp r8,r0
 778:	5f 00       	jmp r9,r15
 77a:	58 00       	jmp r9,r8
 77c:	47 00       	jmp r8,r7
 77e:	51 00       	jmp r9,r1
 780:	57 00       	jmp r9,r7
 782:	55 00       	jmp r9,r5
 784:	4c 00       	jmp r8,r12

00000786 <jmpf>:
 786:	00 02 00 00 	jmpf 0x0
 78a:	ff 02 ff ff 	jmpf 0xffffff
 78e:	00 02 00 80 	jmpf 0x800000
 792:	ff 02 ff 7f 	jmpf 0x7fffff
 796:	01 02 00 00 	jmpf 0x1
 79a:	6d 02 c0 a3 	jmpf 0xa3c06d
 79e:	52 02 54 e6 	jmpf 0xe65452
 7a2:	d8 02 56 16 	jmpf 0x1656d8

000007a6 <callrgr>:
 7a6:	10 00       	callr r0
 7a8:	1f 00       	callr r15
 7aa:	18 00       	callr r8
 7ac:	17 00       	callr r7
 7ae:	11 00       	callr r1
 7b0:	11 00       	callr r1
 7b2:	1c 00       	callr r12
 7b4:	18 00       	callr r8

000007b6 <callgr>:
 7b6:	a0 00       	call r8,r0
 7b8:	bf 00       	call r9,r15
 7ba:	b8 00       	call r9,r8
 7bc:	a7 00       	call r8,r7
 7be:	b1 00       	call r9,r1
 7c0:	b6 00       	call r9,r6
 7c2:	be 00       	call r9,r14
 7c4:	ac 00       	call r8,r12

000007c6 <callfimm>:
 7c6:	00 01 00 00 	callf 0x0
 7ca:	ff 01 ff ff 	callf 0xffffff
 7ce:	00 01 00 80 	callf 0x800000
 7d2:	ff 01 ff 7f 	callf 0x7fffff
 7d6:	01 01 00 00 	callf 0x1
 7da:	56 01 b2 ce 	callf 0xceb256
 7de:	df 01 5f a5 	callf 0xa55fdf
 7e2:	b3 01 e6 e7 	callf 0xe7e6b3

000007e6 <icallrgr>:
 7e6:	30 00       	icallr r0
 7e8:	3f 00       	icallr r15
 7ea:	38 00       	icallr r8
 7ec:	37 00       	icallr r7
 7ee:	31 00       	icallr r1
 7f0:	3f 00       	icallr r15
 7f2:	3c 00       	icallr r12
 7f4:	39 00       	icallr r9

000007f6 <icallgr>:
 7f6:	60 00       	icall r8,r0
 7f8:	7f 00       	icall r9,r15
 7fa:	78 00       	icall r9,r8
 7fc:	67 00       	icall r8,r7
 7fe:	71 00       	icall r9,r1
 800:	7a 00       	icall r9,r10
 802:	6f 00       	icall r8,r15
 804:	6a 00       	icall r8,r10

00000806 <icallfimm>:
 806:	00 03 00 00 	icallf 0x0
 80a:	ff 03 ff ff 	icallf 0xffffff
 80e:	00 03 00 80 	icallf 0x800000
 812:	ff 03 ff 7f 	icallf 0x7fffff
 816:	01 03 00 00 	icallf 0x1
 81a:	22 03 3f 93 	icallf 0x933f22
 81e:	6e 03 35 1e 	icallf 0x1e356e
 822:	48 03 e8 74 	icallf 0x74e848

00000826 <iret>:
 826:	02 00       	iret

00000828 <ret>:
 828:	03 00       	ret

0000082a <mul>:
 82a:	d0 00       	mul

0000082c <div>:
 82c:	c0 00       	div

0000082e <sdiv>:
 82e:	c8 00       	sdiv

00000830 <divlh>:
 830:	e0 00       	divlh

00000832 <sdivlh>:
 832:	e8 00       	sdivlh

00000834 <nop>:
 834:	00 00       	nop
 836:	03 00       	ret

00000838 <halt>:
 838:	08 00       	halt

0000083a <hold>:
 83a:	0a 00       	hold

0000083c <holdx>:
 83c:	0b 00       	holdx

0000083e <brk>:
 83e:	05 00       	brk

00000840 <bccgrgr>:
 840:	00 0d 00 00 	bge r0,r0,0x844
 844:	ff 0d ff ff 	bz r15,r15,0x847
 848:	88 0d 00 88 	bpl r8,r8,0x4c
 84c:	77 0d ff 77 	bls r7,r7,0x104f
 850:	11 0d 01 10 	bnc r1,r1,0x855
 854:	d3 0d 07 37 	bc r3,r13,0xf5f
 858:	a1 0d 1d 08 	bge r1,r10,0x79
 85c:	50 0d 94 fb 	bz r0,r5,0x3f4

00000860 <bccgrimm8>:
 860:	00 20 00 00 	bge r0,#0x0,0x864
 864:	ff 2e ff ff 	bz r7,#0xff,0x867
 868:	80 28 00 88 	bpl r4,#0x80,0x6c
 86c:	7f 26 ff 77 	bls r3,#0x7f,0x106f
 870:	01 22 01 10 	bnc r1,#0x1,0x875
 874:	08 26 c1 15 	bnc r3,#0x8,0xe39
 878:	cb 2a 53 c6 	bnz\.b r5,#0xcb,0xecf
 87c:	e1 2e d2 33 	bc r7,#0xe1,0xc52

00000880 <bccimm16>:
 880:	00 c0 00 00 	bge Rx,#0x0,0x884
 884:	ff cf ff ff 	bz Rx,#0xffff,0x887
 888:	80 c8 00 80 	bpl Rx,#0x8000,0x80c
 88c:	7f c7 ff 7f 	bls Rx,#0x7fff,0x90f
 890:	01 c1 01 00 	bnc Rx,#0x1,0x895
 894:	04 ce fb 77 	bz\.b Rx,#0x77fb,0x89c
 898:	f3 c9 3a f3 	bnv Rx,#0xf33a,0x88f
 89c:	6c c9 32 bc 	bnv Rx,#0xbc32,0x90c

000008a0 <bngrimm4>:
 8a0:	00 04 00 00 	bn r0,#0x0,0x8a4
 8a4:	ff 04 ff 0f 	bn r15,#0xf,0x8a7
 8a8:	88 04 00 08 	bn r8,#0x8,0xac
 8ac:	77 04 ff 07 	bn r7,#0x7,0x10af
 8b0:	11 04 01 00 	bn r1,#0x1,0x8b5
 8b4:	3b 04 49 08 	bn r11,#0x3,0x101
 8b8:	4f 04 4b 0b 	bn r15,#0x4,0x407
 8bc:	8a 04 9b 06 	bn r10,#0x8,0xf5b

000008c0 <bngrgr>:
 8c0:	00 06 00 00 	bn r0,r0,0x8c4
 8c4:	ff 06 ff 0f 	bn r15,r15,0x8c7
 8c8:	88 06 00 08 	bn r8,r8,0xcc
 8cc:	77 06 ff 07 	bn r7,r7,0x10cf
 8d0:	11 06 01 00 	bn r1,r1,0x8d5
 8d4:	34 06 9d 04 	bn r4,r3,0xd75
 8d8:	25 06 4d 00 	bn r5,r2,0x929
 8dc:	73 06 77 02 	bn r3,r7,0xb57

000008e0 <bnlmemimm>:
 8e0:	00 7c 00 00 	bn 0x0,#0x0,0x8e4
 8e4:	ff 7c ff 7f 	bn 0xff,#0x7,0x8e7
 8e8:	80 7c 00 48 	bn 0x80,#0x4,0xec
 8ec:	7f 7c ff 37 	bn 0x7f,#0x3,0x10ef
 8f0:	01 7c 01 10 	bn 0x1,#0x1,0x8f5
 8f4:	99 7c b1 7c 	bn 0x99,#0x7,0x5a9
 8f8:	cc 7c a7 08 	bn 0xcc,#0x0,0x1a3
 8fc:	f2 7c 74 75 	bn 0xf2,#0x7,0xe74

00000900 <bnhmemimm>:
 900:	00 7e 00 00 	bn 0x7f00,#0x0,0x904
 904:	ff 7e ff 7f 	bn 0x7fff,#0x7,0x907
 908:	80 7e 00 48 	bn 0x7f80,#0x4,0x10c
 90c:	7f 7e ff 37 	bn 0x7f7f,#0x3,0x110f
 910:	01 7e 01 10 	bn 0x7f01,#0x1,0x915
 914:	b9 7e 9a 3d 	bn 0x7fb9,#0x3,0x6b2
 918:	69 7e 64 1d 	bn 0x7f69,#0x1,0x680
 91c:	4f 7e 20 75 	bn 0x7f4f,#0x7,0xe40

00000920 <bpgrimm4>:
 920:	00 05 00 00 	bp r0,#0x0,0x924
 924:	ff 05 ff 0f 	bp r15,#0xf,0x927
 928:	88 05 00 08 	bp r8,#0x8,0x12c
 92c:	77 05 ff 07 	bp r7,#0x7,0x112f
 930:	11 05 01 00 	bp r1,#0x1,0x935
 934:	c0 05 33 04 	bp r0,#0xc,0xd6b
 938:	51 05 27 02 	bp r1,#0x5,0xb63
 93c:	86 05 34 06 	bp r6,#0x8,0xf74

00000940 <bpgrgr>:
 940:	00 07 00 00 	bp r0,r0,0x944
 944:	ff 07 ff 0f 	bp r15,r15,0x947
 948:	88 07 00 08 	bp r8,r8,0x14c
 94c:	77 07 ff 07 	bp r7,r7,0x114f
 950:	11 07 01 00 	bp r1,r1,0x955
 954:	94 07 9a 0d 	bp r4,r9,0x6f2
 958:	a9 07 b0 0a 	bp r9,r10,0x40c
 95c:	14 07 97 01 	bp r4,r1,0xaf7

00000960 <bplmemimm>:
 960:	00 7d 00 00 	bp 0x0,#0x0,0x964
 964:	ff 7d ff 7f 	bp 0xff,#0x7,0x967
 968:	80 7d 00 48 	bp 0x80,#0x4,0x16c
 96c:	7f 7d ff 37 	bp 0x7f,#0x3,0x116f
 970:	01 7d 01 10 	bp 0x1,#0x1,0x975
 974:	c1 7d 72 3e 	bp 0xc1,#0x3,0x7ea
 978:	fa 7d ef 29 	bp 0xfa,#0x2,0x36b
 97c:	b4 7d 43 62 	bp 0xb4,#0x6,0xbc3

00000980 <bphmemimm>:
 980:	00 7f 00 00 	bp 0x7f00,#0x0,0x984
 984:	ff 7f ff 7f 	bp 0x7fff,#0x7,0x987
 988:	80 7f 00 48 	bp 0x7f80,#0x4,0x18c
 98c:	7f 7f ff 37 	bp 0x7f7f,#0x3,0x118f
 990:	01 7f 01 10 	bp 0x7f01,#0x1,0x995
 994:	c3 7f 50 1e 	bp 0x7fc3,#0x1,0x7e8
 998:	81 7f 1c 5a 	bp 0x7f81,#0x5,0x3b8
 99c:	38 7f bb 36 	bp 0x7f38,#0x3,0x105b

000009a0 <bcc>:
 9a0:	00 d0       	bge 0x9a2
 9a2:	ff df       	bz 0x9a3
 9a4:	80 d8       	bpl 0x926
 9a6:	7f d7       	bls 0xa27
 9a8:	01 d1       	bnc 0x9ab
 9aa:	30 dc       	bnz\.b 0x9dc
 9ac:	f9 d1       	bnc 0x9a7
 9ae:	4a dc       	bnz\.b 0x9fa

000009b0 <br>:
 9b0:	00 10       	br 0x9b2
 9b2:	fe 1f       	br 0x9b2
 9b4:	00 18       	br 0x1b6
 9b6:	fe 17       	br 0x11b6
 9b8:	00 10       	br 0x9ba
 9ba:	c0 15       	br 0xf7c
 9bc:	52 16       	br 0x1010
 9be:	d2 13       	br 0xd92

000009c0 <callrimm>:
 9c0:	01 10       	callr 0x9c2
 9c2:	ff 1f       	callr 0x9c2
 9c4:	01 18       	callr 0x1c6
 9c6:	ff 17       	callr 0x11c6
 9c8:	01 10       	callr 0x9ca
 9ca:	c1 15       	callr 0xf8c
 9cc:	53 16       	callr 0x1020
 9ce:	d3 13       	callr 0xda2

000009d0 <movgrgrsi>:
 9d0:	08 70 00 00 	mov\.b r0,\(r0,0\)
			9d2: R_XSTORMY16_12	extsym
 9d4:	ff 71 00 00 	mov\.w r7,\(r15,0\)
			9d6: R_XSTORMY16_12	extsym\+0xffffffff
 9d8:	8c 71 00 00 	mov\.w r4,\(r8,0\)
			9da: R_XSTORMY16_12	extsym\+0xfffff800
 9dc:	7b 70 00 00 	mov\.b r3,\(r7,0\)
			9de: R_XSTORMY16_12	extsym\+0x7ff
 9e0:	19 71 00 00 	mov\.w r1,\(r1,0\)
			9e2: R_XSTORMY16_12	extsym\+0x1
 9e4:	8e 71 00 00 	mov\.w r6,\(r8,0\)
			9e6: R_XSTORMY16_12	extsym\+0xfffffe3c
 9e8:	bc 71 00 00 	mov\.w r4,\(r11,0\)
			9ea: R_XSTORMY16_12	extsym\+0x23c
 9ec:	19 70 00 00 	mov\.b r1,\(r1,0\)
			9ee: R_XSTORMY16_12	extsym\+0xfffff94a

000009f0 <movgrgrsipostinc>:
 9f0:	08 60 00 00 	mov\.b r0,\(r0\+\+,0\)
			9f2: R_XSTORMY16_12	extsym
 9f4:	ff 61 00 00 	mov\.w r7,\(r15\+\+,0\)
			9f6: R_XSTORMY16_12	extsym\+0xffffffff
 9f8:	8c 61 00 00 	mov\.w r4,\(r8\+\+,0\)
			9fa: R_XSTORMY16_12	extsym\+0xfffff800
 9fc:	7b 60 00 00 	mov\.b r3,\(r7\+\+,0\)
			9fe: R_XSTORMY16_12	extsym\+0x7ff
 a00:	19 61 00 00 	mov\.w r1,\(r1\+\+,0\)
			a02: R_XSTORMY16_12	extsym\+0x1
 a04:	0e 61 00 00 	mov\.w r6,\(r0\+\+,0\)
			a06: R_XSTORMY16_12	extsym\+0xffffffc0
 a08:	ff 60 00 00 	mov\.b r7,\(r15\+\+,0\)
			a0a: R_XSTORMY16_12	extsym\+0x424
 a0c:	78 60 00 00 	mov\.b r0,\(r7\+\+,0\)
			a0e: R_XSTORMY16_12	extsym\+0x34f

00000a10 <movgrgrsipredec>:
 a10:	08 68 00 00 	mov\.b r0,\(--r0,0\)
			a12: R_XSTORMY16_12	extsym
 a14:	ff 69 00 00 	mov\.w r7,\(--r15,0\)
			a16: R_XSTORMY16_12	extsym\+0xffffffff
 a18:	8c 69 00 00 	mov\.w r4,\(--r8,0\)
			a1a: R_XSTORMY16_12	extsym\+0xfffff800
 a1c:	7b 68 00 00 	mov\.b r3,\(--r7,0\)
			a1e: R_XSTORMY16_12	extsym\+0x7ff
 a20:	19 69 00 00 	mov\.w r1,\(--r1,0\)
			a22: R_XSTORMY16_12	extsym\+0x1
 a24:	f8 69 00 00 	mov\.w r0,\(--r15,0\)
			a26: R_XSTORMY16_12	extsym\+0x6f4
 a28:	1e 69 00 00 	mov\.w r6,\(--r1,0\)
			a2a: R_XSTORMY16_12	extsym\+0x5e2
 a2c:	3f 69 00 00 	mov\.w r7,\(--r3,0\)
			a2e: R_XSTORMY16_12	extsym\+0xfffff80f

00000a30 <movgrsigr>:
 a30:	08 72 00 00 	mov\.b \(r0,0\),r0
			a32: R_XSTORMY16_12	extsym
 a34:	ff 73 00 00 	mov\.w \(r15,0\),r7
			a36: R_XSTORMY16_12	extsym\+0xffffffff
 a38:	8c 73 00 00 	mov\.w \(r8,0\),r4
			a3a: R_XSTORMY16_12	extsym\+0xfffff800
 a3c:	7b 72 00 00 	mov\.b \(r7,0\),r3
			a3e: R_XSTORMY16_12	extsym\+0x7ff
 a40:	19 73 00 00 	mov\.w \(r1,0\),r1
			a42: R_XSTORMY16_12	extsym\+0x1
 a44:	7d 73 00 00 	mov\.w \(r7,0\),r5
			a46: R_XSTORMY16_12	extsym\+0x79c
 a48:	3c 72 00 00 	mov\.b \(r3,0\),r4
			a4a: R_XSTORMY16_12	extsym\+0xfffffcb4
 a4c:	f8 73 00 00 	mov\.w \(r15,0\),r0
			a4e: R_XSTORMY16_12	extsym\+0x6a8

00000a50 <movgrsipostincgr>:
 a50:	08 62 00 00 	mov\.b \(r0\+\+,0\),r0
			a52: R_XSTORMY16_12	extsym
 a54:	ff 63 00 00 	mov\.w \(r15\+\+,0\),r7
			a56: R_XSTORMY16_12	extsym\+0xffffffff
 a58:	8c 63 00 00 	mov\.w \(r8\+\+,0\),r4
			a5a: R_XSTORMY16_12	extsym\+0xfffff800
 a5c:	7b 62 00 00 	mov\.b \(r7\+\+,0\),r3
			a5e: R_XSTORMY16_12	extsym\+0x7ff
 a60:	19 63 00 00 	mov\.w \(r1\+\+,0\),r1
			a62: R_XSTORMY16_12	extsym\+0x1
 a64:	2f 63 00 00 	mov\.w \(r2\+\+,0\),r7
			a66: R_XSTORMY16_12	extsym\+0xffffff50
 a68:	8c 63 00 00 	mov\.w \(r8\+\+,0\),r4
			a6a: R_XSTORMY16_12	extsym\+0x56d
 a6c:	38 62 00 00 	mov\.b \(r3\+\+,0\),r0
			a6e: R_XSTORMY16_12	extsym\+0x2f

00000a70 <movgrsipredecgr>:
 a70:	08 6a 00 00 	mov\.b \(--r0,0\),r0
			a72: R_XSTORMY16_12	extsym
 a74:	ff 6b 00 00 	mov\.w \(--r15,0\),r7
			a76: R_XSTORMY16_12	extsym\+0xffffffff
 a78:	8c 6b 00 00 	mov\.w \(--r8,0\),r4
			a7a: R_XSTORMY16_12	extsym\+0xfffff800
 a7c:	7b 6a 00 00 	mov\.b \(--r7,0\),r3
			a7e: R_XSTORMY16_12	extsym\+0x7ff
 a80:	19 6b 00 00 	mov\.w \(--r1,0\),r1
			a82: R_XSTORMY16_12	extsym\+0x1
 a84:	8c 6a 00 00 	mov\.b \(--r8,0\),r4
			a86: R_XSTORMY16_12	extsym\+0x3ec
 a88:	ea 6b 00 00 	mov\.w \(--r14,0\),r2
			a8a: R_XSTORMY16_12	extsym\+0xfffffa5c
 a8c:	5c 6a 00 00 	mov\.b \(--r5,0\),r4
			a8e: R_XSTORMY16_12	extsym\+0xfffffc61
