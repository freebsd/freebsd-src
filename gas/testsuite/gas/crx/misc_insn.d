#as:
#objdump: -dr
#name: misc_insn

.*: +file format .*

Disassembly of section .text:

00000000 <macsb>:
   0:	08 30 01 40 	macsb	r0, r1

00000004 <macub>:
   4:	08 30 23 41 	macub	r2, r3

00000008 <macqb>:
   8:	08 30 45 42 	macqb	r4, r5

0000000c <macsw>:
   c:	08 30 67 50 	macsw	r6, r7

00000010 <macuw>:
  10:	08 30 89 51 	macuw	r8, r9

00000014 <macqw>:
  14:	08 30 ab 52 	macqw	r10, r11

00000018 <macsd>:
  18:	08 30 cd 60 	macsd	r12, r13

0000001c <macud>:
  1c:	08 30 ef 61 	macud	r14, r15

00000020 <macqd>:
  20:	08 30 ef 62 	macqd	r14, r15

00000024 <mullsd>:
  24:	08 30 02 65 	mullsd	r0, r2

00000028 <mullud>:
  28:	08 30 13 66 	mullud	r1, r3

0000002c <mulsbw>:
  2c:	08 30 46 3b 	mulsbw	r4, r6

00000030 <mulubw>:
  30:	08 30 57 3c 	mulubw	r5, r7

00000034 <mulswd>:
  34:	08 30 8a 3d 	mulswd	r8, r10

00000038 <muluwd>:
  38:	08 30 9b 3e 	muluwd	r9, r11

0000003c <sextbw>:
  3c:	08 30 ce 30 	sextbw	r12, r14

00000040 <sextbd>:
  40:	08 30 df 31 	sextbd	r13, r15

00000044 <sextwd>:
  44:	08 30 ef 32 	sextwd	r14, r15

00000048 <zextbw>:
  48:	08 30 50 34 	zextbw	r5, r0

0000004c <zextbd>:
  4c:	08 30 a6 35 	zextbd	r10, r6

00000050 <zextwd>:
  50:	08 30 7f 36 	zextwd	r7, r15

00000054 <getrfid>:
  54:	9e ff       	getrfid	r14

00000056 <setrfid>:
  56:	af ff       	setrfid	r15

00000058 <bswap>:
  58:	08 30 e2 3f 	bswap	r14, r2

0000005c <maxsb>:
  5c:	08 30 83 80 	maxsb	r8, r3

00000060 <minsb>:
  60:	08 30 fe 81 	minsb	r15, r14

00000064 <maxub>:
  64:	08 30 dc 82 	maxub	r13, r12

00000068 <minub>:
  68:	08 30 ba 83 	minub	r11, r10

0000006c <absb>:
  6c:	08 30 98 84 	absb	r9, r8

00000070 <negb>:
  70:	08 30 76 85 	negb	r7, r6

00000074 <cntl0b>:
  74:	08 30 54 86 	cntl0b	r5, r4

00000078 <cntl1b>:
  78:	08 30 32 87 	cntl1b	r3, r2

0000007c <popcntb>:
  7c:	08 30 10 88 	popcntb	r1, r0

00000080 <rotlb>:
  80:	08 30 b4 89 	rotlb	r11, r4

00000084 <rotrb>:
  84:	08 30 72 8a 	rotrb	r7, r2

00000088 <mulqb>:
  88:	08 30 ee 8b 	mulqb	r14, r14

0000008c <addqb>:
  8c:	08 30 ff 8c 	addqb	r15, r15

00000090 <subqb>:
  90:	08 30 0a 8d 	subqb	r0, r10

00000094 <cntlsb>:
  94:	08 30 2c 8e 	cntlsb	r2, r12

00000098 <maxsw>:
  98:	08 30 83 90 	maxsw	r8, r3

0000009c <minsw>:
  9c:	08 30 fe 91 	minsw	r15, r14

000000a0 <maxuw>:
  a0:	08 30 dc 92 	maxuw	r13, r12

000000a4 <minuw>:
  a4:	08 30 ba 93 	minuw	r11, r10

000000a8 <absw>:
  a8:	08 30 98 94 	absw	r9, r8

000000ac <negw>:
  ac:	08 30 76 95 	negw	r7, r6

000000b0 <cntl0w>:
  b0:	08 30 54 96 	cntl0w	r5, r4

000000b4 <cntl1w>:
  b4:	08 30 32 97 	cntl1w	r3, r2

000000b8 <popcntw>:
  b8:	08 30 10 98 	popcntw	r1, r0

000000bc <rotlw>:
  bc:	08 30 b4 99 	rotlw	r11, r4

000000c0 <rotrw>:
  c0:	08 30 72 9a 	rotrw	r7, r2

000000c4 <mulqw>:
  c4:	08 30 ee 9b 	mulqw	r14, r14

000000c8 <addqw>:
  c8:	08 30 ff 9c 	addqw	r15, r15

000000cc <subqw>:
  cc:	08 30 0a 9d 	subqw	r0, r10

000000d0 <cntlsw>:
  d0:	08 30 2c 9e 	cntlsw	r2, r12

000000d4 <maxsd>:
  d4:	08 30 83 a0 	maxsd	r8, r3

000000d8 <minsd>:
  d8:	08 30 fe a1 	minsd	r15, r14

000000dc <maxud>:
  dc:	08 30 dc a2 	maxud	r13, r12

000000e0 <minud>:
  e0:	08 30 ba a3 	minud	r11, r10

000000e4 <absd>:
  e4:	08 30 98 a4 	absd	r9, r8

000000e8 <negd>:
  e8:	08 30 76 a5 	negd	r7, r6

000000ec <cntl0d>:
  ec:	08 30 54 a6 	cntl0d	r5, r4

000000f0 <cntl1d>:
  f0:	08 30 32 a7 	cntl1d	r3, r2

000000f4 <popcntd>:
  f4:	08 30 10 a8 	popcntd	r1, r0

000000f8 <rotld>:
  f8:	08 30 b4 a9 	rotld	r11, r4

000000fc <rotrd>:
  fc:	08 30 72 aa 	rotrd	r7, r2

00000100 <mulqd>:
 100:	08 30 ee ab 	mulqd	r14, r14

00000104 <addqd>:
 104:	08 30 ff ac 	addqd	r15, r15

00000108 <subqd>:
 108:	08 30 0a ad 	subqd	r0, r10

0000010c <cntlsd>:
 10c:	08 30 2c ae 	cntlsd	r2, r12

00000110 <excp>:
 110:	f8 ff       	excp	bpt
 112:	f5 ff       	excp	svc

00000114 <ram>:
 114:	61 3e ec 21 	ram	\$0x18, \$0x9, \$0x1, r14, r12

00000118 <rim>:
 118:	fd 3e 21 ee 	rim	\$0x1f, \$0xf, \$0xe, r2, r1

0000011c <rotb>:
 11c:	f1 fd       	rotb	\$0x7, r1

0000011e <rotw>:
 11e:	d3 b9       	rotw	\$0xd, r3

00000120 <rotd>:
 120:	08 30 b2 f1 	rotd	\$0x1b, r2
