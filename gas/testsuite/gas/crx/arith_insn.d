#as:
#objdump: -dr
#name: arith_insn

.*: +file format .*

Disassembly of section .text:

00000000 <addub>:
   0:	01 00       	addub	\$0x0, r1
   2:	e2 00 05 00 	addub	\$0x5, r2
   6:	34 40       	addub	r3, r4

00000008 <addb>:
   8:	15 01       	addb	\$0x1, r5
   a:	e6 01 06 00 	addb	\$0x6, r6
   e:	78 41       	addb	r7, r8

00000010 <addcb>:
  10:	29 02       	addcb	\$0x2, r9
  12:	ea 02 09 00 	addcb	\$0x9, r10
  16:	bc 42       	addcb	r11, r12

00000018 <andb>:
  18:	3d 03       	andb	\$0x3, r13
  1a:	9e 03       	andb	\$0x10, r14
  1c:	fe 43       	andb	r15, r14

0000001e <cmpb>:
  1e:	4f 04       	cmpb	\$0x4, r15
  20:	e1 04 11 00 	cmpb	\$0x11, r1
  24:	23 44       	cmpb	r2, r3

00000026 <movb>:
  26:	54 05       	movb	\$0xfffffffc, r4
  28:	e5 05 36 02 	movb	\$0x236, r5
  2c:	67 45       	movb	r6, r7

0000002e <orb>:
  2e:	68 06       	orb	\$0xffffffff, r8
  30:	e9 06 80 69 	orb	\$0x6980, r9
  34:	ab 46       	orb	r10, r11

00000036 <subb>:
  36:	7c 07       	subb	\$0x7, r12
  38:	ed 07 ff 7f 	subb	\$0x7fff, r13
  3c:	ef 47       	subb	r14, r15

0000003e <subcb>:
  3e:	8e 08       	subcb	\$0x8, r14
  40:	ef 08 aa ff 	subcb	\$0xffaa, r15
  44:	12 48       	subcb	r1, r2

00000046 <xorb>:
  46:	e3 09 16 00 	xorb	\$0x16, r3
  4a:	e4 09 02 90 	xorb	\$0x9002, r4
  4e:	56 49       	xorb	r5, r6

00000050 <mulb>:
  50:	e7 0a 32 00 	mulb	\$0x32, r7
  54:	e8 0a fa 0e 	mulb	\$0xefa, r8
  58:	9a 4a       	mulb	r9, r10

0000005a <adduw>:
  5a:	ab 10       	adduw	\$0x20, r11
  5c:	ec 10 ff 7f 	adduw	\$0x7fff, r12
  60:	de 50       	adduw	r13, r14

00000062 <addw>:
  62:	ef 11 12 00 	addw	\$0x12, r15
  66:	ee 11 01 80 	addw	\$0x8001, r14
  6a:	f1 51       	addw	r15, r1

0000006c <addcw>:
  6c:	e2 12 48 00 	addcw	\$0x48, r2
  70:	e3 12 1b 00 	addcw	\$0x1b, r3
  74:	45 52       	addcw	r4, r5

00000076 <andw>:
  76:	06 13       	andw	\$0x0, r6
  78:	e7 13 e5 ff 	andw	\$0xffe5, r7
  7c:	89 53       	andw	r8, r9

0000007e <cmpw>:
  7e:	1a 14       	cmpw	\$0x1, r10
  80:	eb 14 11 00 	cmpw	\$0x11, r11
  84:	cd 54       	cmpw	r12, r13

00000086 <movw>:
  86:	2e 15       	movw	\$0x2, r14
  88:	ef 15 00 0e 	movw	\$0xe00, r15
  8c:	ef 55       	movw	r14, r15

0000008e <orw>:
  8e:	31 16       	orw	\$0x3, r1
  90:	e2 16 fe ff 	orw	\$0xfffe, r2
  94:	34 56       	orw	r3, r4

00000096 <subw>:
  96:	45 17       	subw	\$0x4, r5
  98:	e6 17 12 00 	subw	\$0x12, r6
  9c:	78 57       	subw	r7, r8

0000009e <subcw>:
  9e:	59 18       	subcw	\$0xfffffffc, r9
  a0:	ea 18 f7 ff 	subcw	\$0xfff7, r10
  a4:	bc 58       	subcw	r11, r12

000000a6 <xorw>:
  a6:	6d 19       	xorw	\$0xffffffff, r13
  a8:	ee 19 21 00 	xorw	\$0x21, r14
  ac:	fe 59       	xorw	r15, r14

000000ae <mulw>:
  ae:	7f 1a       	mulw	\$0x7, r15
  b0:	e1 1a 17 00 	mulw	\$0x17, r1
  b4:	23 5a       	mulw	r2, r3

000000b6 <addud>:
  b6:	01 20       	addud	\$0x0, r1
  b8:	e2 20 05 00 	addud	\$0x5, r2
  bc:	f2 20 05 00 	addud	\$0x55555, r2
  c0:	55 55 
  c2:	34 60       	addud	r3, r4

000000c4 <addd>:
  c4:	15 21       	addd	\$0x1, r5
  c6:	e6 21 06 00 	addd	\$0x6, r6
  ca:	f6 21 ff 7f 	addd	\$0x7fffffff, r6
  ce:	ff ff 
  d0:	78 61       	addd	r7, r8

000000d2 <addcd>:
  d2:	29 22       	addcd	\$0x2, r9
  d4:	ea 22 09 00 	addcd	\$0x9, r10
  d8:	fa 22 00 80 	addcd	\$0x80000001, r10
  dc:	01 00 
  de:	bc 62       	addcd	r11, r12

000000e0 <andd>:
  e0:	3d 23       	andd	\$0x3, r13
  e2:	9e 23       	andd	\$0x10, r14
  e4:	6e 23       	andd	\$0xffffffff, r14
  e6:	fe 63       	andd	r15, r14

000000e8 <cmpd>:
  e8:	4f 24       	cmpd	\$0x4, r15
  ea:	e1 24 11 00 	cmpd	\$0x11, r1
  ee:	f1 24 00 f0 	cmpd	\$0xf0000001, r1
  f2:	01 00 
  f4:	23 64       	cmpd	r2, r3

000000f6 <movd>:
  f6:	54 25       	movd	\$0xfffffffc, r4
  f8:	e5 25 36 02 	movd	\$0x236, r5
  fc:	f5 25 00 80 	movd	\$0x80000000, r5
 100:	00 00 
 102:	67 65       	movd	r6, r7

00000104 <ord>:
 104:	68 26       	ord	\$0xffffffff, r8
 106:	e9 26 80 69 	ord	\$0x6980, r9
 10a:	f9 26 01 00 	ord	\$0x10000, r9
 10e:	00 00 
 110:	ab 66       	ord	r10, r11

00000112 <subd>:
 112:	7c 27       	subd	\$0x7, r12
 114:	ed 27 ff 7f 	subd	\$0x7fff, r13
 118:	fd 27 ff ff 	subd	\$0xffff0000, r13
 11c:	00 00 
 11e:	ef 67       	subd	r14, r15

00000120 <subcd>:
 120:	8e 28       	subcd	\$0x8, r14
 122:	ef 28 aa ff 	subcd	\$0xffaa, r15
 126:	6f 28       	subcd	\$0xffffffff, r15
 128:	12 68       	subcd	r1, r2

0000012a <xord>:
 12a:	e3 29 16 00 	xord	\$0x16, r3
 12e:	e4 29 02 90 	xord	\$0x9002, r4
 132:	f4 29 ff 7f 	xord	\$0x7fffffff, r4
 136:	ff ff 
 138:	56 69       	xord	r5, r6

0000013a <muld>:
 13a:	e7 2a 32 00 	muld	\$0x32, r7
 13e:	e8 2a fa 0e 	muld	\$0xefa, r8
 142:	f8 2a 00 80 	muld	\$0x80000001, r8
 146:	01 00 
 148:	9a 6a       	muld	r9, r10
