#as:
#objdump: -dr
#name: bit_insn

.*: +file format .*

Disassembly of section .text:

00000000 <cbitb>:
   0:	06 39 00 00 	cbitb	\$0x6, 0x450 [-_<>+0-9a-z]*
   4:	50 04 
   6:	06 38 50 04 	cbitb	\$0x6, 0xffff0450 [-_<>+0-9a-z]*
   a:	07 39 04 00 	cbitb	\$0x7, 0x41287 [-_<>+0-9a-z]*
   e:	87 12 
  10:	03 3a 09 50 	cbitb	\$0x3, 0x9\(r5\)
  14:	0f fe       	cbitb	\$0x0, \(r15\)
  16:	02 3b ff 10 	cbitb	\$0x2, 0xffffe1\(r1\)
  1a:	e1 ff 
  1c:	04 3d 00 ef 	cbitb	\$0x4, 0xfa\(r14,r15,1\)
  20:	fa 00 
  22:	07 3d ff f7 	cbitb	\$0x7, 0x3ffeb3\(r15,r7,8\)
  26:	b3 fe 

00000028 <cbitw>:
  28:	2f 39 00 00 	cbitw	\$0xf, 0x23 [-_<>+0-9a-z]*
  2c:	23 00 
  2e:	26 38 23 00 	cbitw	\$0x6, 0xffff0023 [-_<>+0-9a-z]*
  32:	21 39 0f 00 	cbitw	\$0x1, 0xff287 [-_<>+0-9a-z]*
  36:	87 f2 
  38:	2f 3a 01 50 	cbitw	\$0xf, 0x1\(r5\)
  3c:	0e bd       	cbitw	\$0x0, \(r14\)
  3e:	25 3b ff 10 	cbitw	\$0x5, 0xffffe1\(r1\)
  42:	e1 ff 
  44:	28 3d 40 ef 	cbitw	\$0x8, 0xaf\(r14,r15,2\)
  48:	af 00 
  4a:	27 3d bf 13 	cbitw	\$0x7, 0x3fff38\(r1,r3,4\)
  4e:	38 ff 

00000050 <cbitd>:
  50:	66 39 00 00 	cbitd	\$0x6, 0xff [-_<>+0-9a-z]*
  54:	ff 00 
  56:	66 38 ff 0f 	cbitd	\$0x6, 0xffff0fff [-_<>+0-9a-z]*
  5a:	7a 39 01 00 	cbitd	\$0x1a, 0x10000 [-_<>+0-9a-z]*
  5e:	00 00 
  60:	7f 3a 07 90 	cbitd	\$0x1f, 0x7\(r9\)
  64:	02 f7       	cbitd	\$0x10, \(r2\)
  66:	7a 3b ff 20 	cbitd	\$0x1a, 0xffffe1\(r2\)
  6a:	e1 ff 
  6c:	7e 3c 0a 3f 	cbitd	\$0x1e, 0xa\(r3,r15,1\)
  70:	67 3d ff 45 	cbitd	\$0x7, 0x3ffb80\(r4,r5,8\)
  74:	80 fb 
  76:	08 30 68 38 	cbitd	r6, r8
  7a:	08 30 e4 f7 	cbitd	\$0x1e, r4

0000007e <sbitb>:
  7e:	0e 39 00 00 	sbitb	\$0x6, 0x450 [-_<>+0-9a-z]*
  82:	50 04 
  84:	0e 38 50 04 	sbitb	\$0x6, 0xffff0450 [-_<>+0-9a-z]*
  88:	0f 39 04 00 	sbitb	\$0x7, 0x41287 [-_<>+0-9a-z]*
  8c:	87 12 
  8e:	0b 3a 09 50 	sbitb	\$0x3, 0x9\(r5\)
  92:	8f fe       	sbitb	\$0x0, \(r15\)
  94:	0a 3b ff 10 	sbitb	\$0x2, 0xffffe1\(r1\)
  98:	e1 ff 
  9a:	0c 3d 00 ef 	sbitb	\$0x4, 0xfa\(r14,r15,1\)
  9e:	fa 00 
  a0:	0f 3d ff f7 	sbitb	\$0x7, 0x3ffeb3\(r15,r7,8\)
  a4:	b3 fe 

000000a6 <sbitw>:
  a6:	3f 39 00 00 	sbitw	\$0xf, 0x23 [-_<>+0-9a-z]*
  aa:	23 00 
  ac:	36 38 23 00 	sbitw	\$0x6, 0xffff0023 [-_<>+0-9a-z]*
  b0:	31 39 0f 00 	sbitw	\$0x1, 0xff287 [-_<>+0-9a-z]*
  b4:	87 f2 
  b6:	3f 3a 01 50 	sbitw	\$0xf, 0x1\(r5\)
  ba:	0e be       	sbitw	\$0x0, \(r14\)
  bc:	35 3b ff 10 	sbitw	\$0x5, 0xffffe1\(r1\)
  c0:	e1 ff 
  c2:	38 3d 40 ef 	sbitw	\$0x8, 0xaf\(r14,r15,2\)
  c6:	af 00 
  c8:	37 3d bf 13 	sbitw	\$0x7, 0x3fff38\(r1,r3,4\)
  cc:	38 ff 

000000ce <sbitd>:
  ce:	86 39 00 00 	sbitd	\$0x6, 0xff [-_<>+0-9a-z]*
  d2:	ff 00 
  d4:	86 38 ff 0f 	sbitd	\$0x6, 0xffff0fff [-_<>+0-9a-z]*
  d8:	9a 39 01 00 	sbitd	\$0x1a, 0x10000 [-_<>+0-9a-z]*
  dc:	00 00 
  de:	9f 3a 07 90 	sbitd	\$0x1f, 0x7\(r9\)
  e2:	02 f9       	sbitd	\$0x10, \(r2\)
  e4:	9a 3b ff 20 	sbitd	\$0x1a, 0xffffe1\(r2\)
  e8:	e1 ff 
  ea:	9e 3c 0a 3f 	sbitd	\$0x1e, 0xa\(r3,r15,1\)
  ee:	87 3d ff 45 	sbitd	\$0x7, 0x3ffb80\(r4,r5,8\)
  f2:	80 fb 
  f4:	08 30 68 39 	sbitd	r6, r8
  f8:	08 30 e4 f9 	sbitd	\$0x1e, r4

000000fc <tbitb>:
  fc:	16 39 00 00 	tbitb	\$0x6, 0x450 [-_<>+0-9a-z]*
 100:	50 04 
 102:	16 38 50 04 	tbitb	\$0x6, 0xffff0450 [-_<>+0-9a-z]*
 106:	17 39 04 00 	tbitb	\$0x7, 0x41287 [-_<>+0-9a-z]*
 10a:	87 12 
 10c:	13 3a 09 50 	tbitb	\$0x3, 0x9\(r5\)
 110:	0f ff       	tbitb	\$0x0, \(r15\)
 112:	12 3b ff 10 	tbitb	\$0x2, 0xffffe1\(r1\)
 116:	e1 ff 
 118:	14 3d 00 ef 	tbitb	\$0x4, 0xfa\(r14,r15,1\)
 11c:	fa 00 
 11e:	17 3d ff f7 	tbitb	\$0x7, 0x3ffeb3\(r15,r7,8\)
 122:	b3 fe 

00000124 <tbitw>:
 124:	4f 39 00 00 	tbitw	\$0xf, 0x23 [-_<>+0-9a-z]*
 128:	23 00 
 12a:	46 38 23 00 	tbitw	\$0x6, 0xffff0023 [-_<>+0-9a-z]*
 12e:	41 39 0f 00 	tbitw	\$0x1, 0xff287 [-_<>+0-9a-z]*
 132:	87 f2 
 134:	4f 3a 01 50 	tbitw	\$0xf, 0x1\(r5\)
 138:	0e bf       	tbitw	\$0x0, \(r14\)
 13a:	45 3b ff 10 	tbitw	\$0x5, 0xffffe1\(r1\)
 13e:	e1 ff 
 140:	48 3d 40 ef 	tbitw	\$0x8, 0xaf\(r14,r15,2\)
 144:	af 00 
 146:	47 3d bf 13 	tbitw	\$0x7, 0x3fff38\(r1,r3,4\)
 14a:	38 ff 

0000014c <tbitd>:
 14c:	a6 39 00 00 	tbitd	\$0x6, 0xff [-_<>+0-9a-z]*
 150:	ff 00 
 152:	a6 38 ff 0f 	tbitd	\$0x6, 0xffff0fff [-_<>+0-9a-z]*
 156:	ba 39 01 00 	tbitd	\$0x1a, 0x10000 [-_<>+0-9a-z]*
 15a:	00 00 
 15c:	bf 3a 07 90 	tbitd	\$0x1f, 0x7\(r9\)
 160:	02 fb       	tbitd	\$0x10, \(r2\)
 162:	ba 3b ff 20 	tbitd	\$0x1a, 0xffffe1\(r2\)
 166:	e1 ff 
 168:	be 3c 0a 3f 	tbitd	\$0x1e, 0xa\(r3,r15,1\)
 16c:	a7 3d ff 45 	tbitd	\$0x7, 0x3ffb80\(r4,r5,8\)
 170:	80 fb 
 172:	08 30 68 3a 	tbitd	r6, r8
 176:	08 30 e4 fb 	tbitd	\$0x1e, r4
