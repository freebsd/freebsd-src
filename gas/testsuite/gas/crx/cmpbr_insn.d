#as:
#objdump: -dr
#name: cmpbr_insn

.*: +file format .*

Disassembly of section .text:

00000000 <cmpbeqb>:
   0:	81 30 2b 20 	cmpbeqb	r1, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
   4:	83 31 00 40 	cmpbeqb	r3, r4, 0x[0-9a-f]* [-_<>+0-9a-z]*
   8:	a4 21 
   a:	c0 30 1b 50 	cmpbeqb	\$0x0, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
   e:	c1 31 1a 60 	cmpbeqb	\$0x1, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
  12:	3c 2b 

00000014 <cmpbneb>:
  14:	87 30 7d 81 	cmpbneb	r7, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
  18:	89 31 00 a1 	cmpbneb	r9, r10, 0x[0-9a-f]* [-_<>+0-9a-z]*
  1c:	00 78 
  1e:	c2 30 01 b1 	cmpbneb	\$0x2, r11, 0x[0-9a-f]* [-_<>+0-9a-z]*
  22:	c3 31 7f c1 	cmpbneb	\$0x3, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
  26:	ff ff 

00000028 <cmpbhib>:
  28:	8d 31 00 e4 	cmpbhib	r13, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
  2c:	80 00 
  2e:	8f 31 00 e4 	cmpbhib	r15, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
  32:	81 00 
  34:	c4 30 ff f4 	cmpbhib	\$0x4, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
  38:	c5 31 ff 14 	cmpbhib	\$0xfffffffc, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  3c:	7e ff 

0000003e <cmpblsb>:
  3e:	82 30 3c 35 	cmpblsb	r2, r3, 0x[0-9a-f]* [-_<>+0-9a-z]*
  42:	84 31 00 55 	cmpblsb	r4, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
  46:	80 00 
  48:	c6 30 84 65 	cmpblsb	\$0xffffffff, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
  4c:	c7 31 ff 75 	cmpblsb	\$0x7, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
  50:	7f ff 

00000052 <cmpbgtb>:
  52:	88 30 83 96 	cmpbgtb	r8, r9, 0x[0-9a-f]* [-_<>+0-9a-z]*
  56:	8a 31 00 b6 	cmpbgtb	r10, r11, 0x[0-9a-f]* [-_<>+0-9a-z]*
  5a:	e0 07 
  5c:	c8 30 7f c6 	cmpbgtb	\$0x8, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
  60:	c9 31 7f d6 	cmpbgtb	\$0x10, r13, 0x[0-9a-f]* [-_<>+0-9a-z]*
  64:	f9 ff 

00000066 <cmpbleb>:
  66:	8e 30 81 f7 	cmpbleb	r14, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
  6a:	8e 31 ff f7 	cmpbleb	r14, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
  6e:	00 ff 
  70:	c9 30 1b 17 	cmpbleb	\$0x10, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  74:	c9 31 80 27 	cmpbleb	\$0x10, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
  78:	7f 00 

0000007a <cmpblob>:
  7a:	83 30 e4 4a 	cmpblob	r3, r4, 0x[0-9a-f]* [-_<>+0-9a-z]*
  7e:	85 31 80 6a 	cmpblob	r5, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
  82:	01 00 
  84:	ca 30 12 7a 	cmpblob	\$0x20, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
  88:	ca 31 7f 8a 	cmpblob	\$0x20, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
  8c:	ff ff 

0000008e <cmpbhsb>:
  8e:	89 30 78 ab 	cmpbhsb	r9, r10, 0x[0-9a-f]* [-_<>+0-9a-z]*
  92:	8b 31 00 cb 	cmpbhsb	r11, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
  96:	81 00 
  98:	ca 30 81 db 	cmpbhsb	\$0x20, r13, 0x[0-9a-f]* [-_<>+0-9a-z]*
  9c:	cb 31 00 eb 	cmpbhsb	\$0x14, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
  a0:	00 08 

000000a2 <cmpbltb>:
  a2:	8f 30 08 ec 	cmpbltb	r15, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
  a6:	8f 31 00 1c 	cmpbltb	r15, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
  aa:	31 02 
  ac:	cc 30 f8 2c 	cmpbltb	\$0xc, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
  b0:	cc 31 c0 3c 	cmpbltb	\$0xc, r3, 0x[0-9a-f]* [-_<>+0-9a-z]*
  b4:	00 00 

000000b6 <cmpbgeb>:
  b6:	84 30 00 5d 	cmpbgeb	r4, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
  ba:	86 31 20 7d 	cmpbgeb	r6, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
  be:	00 00 
  c0:	cd 30 00 8d 	cmpbgeb	\$0x30, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
  c4:	cd 31 f8 9d 	cmpbgeb	\$0x30, r9, 0x[0-9a-f]* [-_<>+0-9a-z]*
  c8:	00 00 

000000ca <cmpbeqw>:
  ca:	91 30 2b 20 	cmpbeqw	r1, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
  ce:	93 31 00 40 	cmpbeqw	r3, r4, 0x[0-9a-f]* [-_<>+0-9a-z]*
  d2:	a4 21 
  d4:	d0 30 1b 50 	cmpbeqw	\$0x0, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
  d8:	d1 31 1a 60 	cmpbeqw	\$0x1, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
  dc:	3c 2b 

000000de <cmpbnew>:
  de:	97 30 7d 81 	cmpbnew	r7, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
  e2:	99 31 00 a1 	cmpbnew	r9, r10, 0x[0-9a-f]* [-_<>+0-9a-z]*
  e6:	00 78 
  e8:	d2 30 01 b1 	cmpbnew	\$0x2, r11, 0x[0-9a-f]* [-_<>+0-9a-z]*
  ec:	d3 31 7f c1 	cmpbnew	\$0x3, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
  f0:	ff ff 

000000f2 <cmpbhiw>:
  f2:	9d 31 00 e4 	cmpbhiw	r13, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
  f6:	80 00 
  f8:	9f 31 00 e4 	cmpbhiw	r15, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
  fc:	81 00 
  fe:	d4 30 ff f4 	cmpbhiw	\$0x4, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
 102:	d5 31 ff 14 	cmpbhiw	\$0xfffffffc, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
 106:	7e ff 

00000108 <cmpblsw>:
 108:	92 30 3c 35 	cmpblsw	r2, r3, 0x[0-9a-f]* [-_<>+0-9a-z]*
 10c:	94 31 00 55 	cmpblsw	r4, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
 110:	80 00 
 112:	d6 30 84 65 	cmpblsw	\$0xffffffff, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
 116:	d7 31 ff 75 	cmpblsw	\$0x7, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
 11a:	7f ff 

0000011c <cmpbgtw>:
 11c:	98 30 83 96 	cmpbgtw	r8, r9, 0x[0-9a-f]* [-_<>+0-9a-z]*
 120:	9a 31 00 b6 	cmpbgtw	r10, r11, 0x[0-9a-f]* [-_<>+0-9a-z]*
 124:	e0 07 
 126:	d8 30 7f c6 	cmpbgtw	\$0x8, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
 12a:	d9 31 7f d6 	cmpbgtw	\$0x10, r13, 0x[0-9a-f]* [-_<>+0-9a-z]*
 12e:	f9 ff 

00000130 <cmpblew>:
 130:	9e 30 81 f7 	cmpblew	r14, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
 134:	9e 31 ff f7 	cmpblew	r14, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
 138:	00 ff 
 13a:	d9 30 1b 17 	cmpblew	\$0x10, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
 13e:	d9 31 80 27 	cmpblew	\$0x10, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
 142:	7f 00 

00000144 <cmpblow>:
 144:	93 30 e4 4a 	cmpblow	r3, r4, 0x[0-9a-f]* [-_<>+0-9a-z]*
 148:	95 31 80 6a 	cmpblow	r5, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
 14c:	01 00 
 14e:	da 30 12 7a 	cmpblow	\$0x20, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
 152:	da 31 7f 8a 	cmpblow	\$0x20, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
 156:	ff ff 

00000158 <cmpbhsw>:
 158:	99 30 78 ab 	cmpbhsw	r9, r10, 0x[0-9a-f]* [-_<>+0-9a-z]*
 15c:	9b 31 00 cb 	cmpbhsw	r11, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
 160:	81 00 
 162:	da 30 81 db 	cmpbhsw	\$0x20, r13, 0x[0-9a-f]* [-_<>+0-9a-z]*
 166:	db 31 00 eb 	cmpbhsw	\$0x14, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
 16a:	00 08 

0000016c <cmpbltw>:
 16c:	9f 30 08 ec 	cmpbltw	r15, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
 170:	9f 31 00 1c 	cmpbltw	r15, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
 174:	31 02 
 176:	dc 30 f8 2c 	cmpbltw	\$0xc, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
 17a:	dc 31 c0 3c 	cmpbltw	\$0xc, r3, 0x[0-9a-f]* [-_<>+0-9a-z]*
 17e:	00 00 

00000180 <cmpbgew>:
 180:	94 30 00 5d 	cmpbgew	r4, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
 184:	96 31 20 7d 	cmpbgew	r6, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
 188:	00 00 
 18a:	dd 30 00 8d 	cmpbgew	\$0x30, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
 18e:	dd 31 f8 9d 	cmpbgew	\$0x30, r9, 0x[0-9a-f]* [-_<>+0-9a-z]*
 192:	00 00 

00000194 <cmpbeqd>:
 194:	a1 30 2b 20 	cmpbeqd	r1, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
 198:	a3 31 00 40 	cmpbeqd	r3, r4, 0x[0-9a-f]* [-_<>+0-9a-z]*
 19c:	a4 21 
 19e:	e0 30 1b 50 	cmpbeqd	\$0x0, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1a2:	e1 31 1a 60 	cmpbeqd	\$0x1, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1a6:	3c 2b 

000001a8 <cmpbned>:
 1a8:	a7 30 7d 81 	cmpbned	r7, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1ac:	a9 31 00 a1 	cmpbned	r9, r10, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1b0:	00 78 
 1b2:	e2 30 01 b1 	cmpbned	\$0x2, r11, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1b6:	e3 31 7f c1 	cmpbned	\$0x3, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1ba:	ff ff 

000001bc <cmpbhid>:
 1bc:	ad 31 00 e4 	cmpbhid	r13, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1c0:	80 00 
 1c2:	af 31 00 e4 	cmpbhid	r15, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1c6:	81 00 
 1c8:	e4 30 ff f4 	cmpbhid	\$0x4, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1cc:	e5 31 ff 14 	cmpbhid	\$0xfffffffc, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1d0:	7e ff 

000001d2 <cmpblsd>:
 1d2:	a2 30 3c 35 	cmpblsd	r2, r3, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1d6:	a4 31 00 55 	cmpblsd	r4, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1da:	80 00 
 1dc:	e6 30 84 65 	cmpblsd	\$0xffffffff, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1e0:	e7 31 ff 75 	cmpblsd	\$0x7, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1e4:	7f ff 

000001e6 <cmpbgtd>:
 1e6:	a8 30 83 96 	cmpbgtd	r8, r9, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1ea:	aa 31 00 b6 	cmpbgtd	r10, r11, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1ee:	e0 07 
 1f0:	e8 30 7f c6 	cmpbgtd	\$0x8, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1f4:	e9 31 7f d6 	cmpbgtd	\$0x10, r13, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1f8:	f9 ff 

000001fa <cmpbled>:
 1fa:	ae 30 81 f7 	cmpbled	r14, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
 1fe:	ae 31 ff f7 	cmpbled	r14, r15, 0x[0-9a-f]* [-_<>+0-9a-z]*
 202:	00 ff 
 204:	e9 30 1b 17 	cmpbled	\$0x10, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
 208:	e9 31 80 27 	cmpbled	\$0x10, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
 20c:	7f 00 

0000020e <cmpblod>:
 20e:	a3 30 e4 4a 	cmpblod	r3, r4, 0x[0-9a-f]* [-_<>+0-9a-z]*
 212:	a5 31 80 6a 	cmpblod	r5, r6, 0x[0-9a-f]* [-_<>+0-9a-z]*
 216:	01 00 
 218:	ea 30 12 7a 	cmpblod	\$0x20, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
 21c:	ea 31 7f 8a 	cmpblod	\$0x20, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
 220:	ff ff 

00000222 <cmpbhsd>:
 222:	a9 30 78 ab 	cmpbhsd	r9, r10, 0x[0-9a-f]* [-_<>+0-9a-z]*
 226:	ab 31 00 cb 	cmpbhsd	r11, r12, 0x[0-9a-f]* [-_<>+0-9a-z]*
 22a:	81 00 
 22c:	ea 30 81 db 	cmpbhsd	\$0x20, r13, 0x[0-9a-f]* [-_<>+0-9a-z]*
 230:	eb 31 00 eb 	cmpbhsd	\$0x14, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
 234:	00 08 

00000236 <cmpbltd>:
 236:	af 30 08 ec 	cmpbltd	r15, r14, 0x[0-9a-f]* [-_<>+0-9a-z]*
 23a:	af 31 00 1c 	cmpbltd	r15, r1, 0x[0-9a-f]* [-_<>+0-9a-z]*
 23e:	31 02 
 240:	ec 30 f8 2c 	cmpbltd	\$0xc, r2, 0x[0-9a-f]* [-_<>+0-9a-z]*
 244:	ec 31 c0 3c 	cmpbltd	\$0xc, r3, 0x[0-9a-f]* [-_<>+0-9a-z]*
 248:	00 00 

0000024a <cmpbged>:
 24a:	a4 30 00 5d 	cmpbged	r4, r5, 0x[0-9a-f]* [-_<>+0-9a-z]*
 24e:	a6 31 20 7d 	cmpbged	r6, r7, 0x[0-9a-f]* [-_<>+0-9a-z]*
 252:	00 00 
 254:	ed 30 00 8d 	cmpbged	\$0x30, r8, 0x[0-9a-f]* [-_<>+0-9a-z]*
 258:	ed 31 f8 9d 	cmpbged	\$0x30, r9, 0x[0-9a-f]* [-_<>+0-9a-z]*
 25c:	00 00 
