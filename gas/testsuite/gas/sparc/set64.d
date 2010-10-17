#as: -64 -Av9
#objdump: -dr
#name: sparc64 set64

.*: +file format .*sparc.*

Disassembly of section .text:

0+ <foo>:
   0:	05 00 00 00 	sethi  %hi\((0x|)0\), %g2
			0: R_SPARC_HI22	.text
   4:	84 10 a0 00 	mov  %g2, %g2	! 0 <foo>
			4: R_SPARC_LO10	.text
   8:	07 1d 95 0c 	sethi  %hi\(0x76543000\), %g3
   c:	86 10 e2 10 	or  %g3, 0x210, %g3	! 76543210 <(\*ABS\*|foo)\+(0x|)0x76543210>
  10:	88 10 20 00 	clr  %g4
  14:	0b 00 00 3f 	sethi  %hi\(0xfc00\), %g5
  18:	8a 11 63 ff 	or  %g5, 0x3ff, %g5	! ffff <(\*ABS\*|foo)\+(0x|)ffff>
  1c:	03 00 00 00 	sethi  %hi\((0x|)0\), %g1
			1c: R_SPARC_HH22	.text
  20:	05 00 00 00 	sethi  %hi\((0x|)0\), %g2
			20: R_SPARC_LM22	.text
  24:	82 10 60 00 	mov  %g1, %g1
			24: R_SPARC_HM10	.text
  28:	84 10 a0 00 	mov  %g2, %g2
			28: R_SPARC_LO10	.text
  2c:	83 28 70 20 	sllx  %g1, 0x20, %g1
  30:	84 10 80 01 	or  %g2, %g1, %g2
  34:	86 10 3f ff 	mov  -1, %g3
  38:	86 10 20 00 	clr  %g3
  3c:	86 10 20 01 	mov  1, %g3
  40:	86 10 2f ff 	mov  0xfff, %g3
  44:	07 00 00 04 	sethi  %hi\(0x1000\), %g3
  48:	86 10 30 00 	mov  -4096, %g3
  4c:	07 00 00 04 	sethi  %hi\(0x1000\), %g3
  50:	86 18 ff ff 	xor  %g3, -1, %g3
  54:	07 00 00 3f 	sethi  %hi\(0xfc00\), %g3
  58:	86 10 e3 ff 	or  %g3, 0x3ff, %g3	! ffff <(\*ABS\*|foo)\+(0x|)ffff>
  5c:	07 00 00 3f 	sethi  %hi\(0xfc00\), %g3
  60:	86 18 fc 00 	xor  %g3, -1024, %g3
  64:	09 1f ff ff 	sethi  %hi\(0x7ffffc00\), %g4
  68:	88 11 23 ff 	or  %g4, 0x3ff, %g4	! 7fffffff <(\*ABS\*|foo)\+(0x|)7fffffff>
  6c:	09 20 00 00 	sethi  %hi\(0x80000000\), %g4
  70:	09 1f ff ff 	sethi  %hi\(0x7ffffc00\), %g4
  74:	88 19 3c 00 	xor  %g4, -1024, %g4
  78:	09 20 00 00 	sethi  %hi\(0x80000000\), %g4
  7c:	88 19 3f ff 	xor  %g4, -1, %g4
  80:	09 3f ff ff 	sethi  %hi\(0xfffffc00\), %g4
  84:	88 11 23 ff 	or  %g4, 0x3ff, %g4	! ffffffff <(\*ABS\*|foo)\+(0x|)ffffffff>
  88:	88 10 20 01 	mov  1, %g4
  8c:	89 29 30 20 	sllx  %g4, 0x20, %g4
  90:	03 1f ff ff 	sethi  %hi\(0x7ffffc00\), %g1
  94:	0b 3f ff ff 	sethi  %hi\(0xfffffc00\), %g5
  98:	82 10 63 ff 	or  %g1, 0x3ff, %g1
  9c:	8a 11 63 ff 	or  %g5, 0x3ff, %g5
  a0:	83 28 70 20 	sllx  %g1, 0x20, %g1
  a4:	8a 11 40 01 	or  %g5, %g1, %g5
  a8:	0b 20 00 00 	sethi  %hi\(0x80000000\), %g5
  ac:	8b 29 70 20 	sllx  %g5, 0x20, %g5
  b0:	0b 3f ff ff 	sethi  %hi\(0xfffffc00\), %g5
  b4:	8a 19 7c 00 	xor  %g5, -1024, %g5
  b8:	0b 1f ff ff 	sethi  %hi\(0x7ffffc00\), %g5
  bc:	8a 19 7c 00 	xor  %g5, -1024, %g5
  c0:	03 3f ff c0 	sethi  %hi\(0xffff0000\), %g1
  c4:	0b 3f ff c0 	sethi  %hi\(0xffff0000\), %g5
  c8:	83 28 70 20 	sllx  %g1, 0x20, %g1
  cc:	8a 11 40 01 	or  %g5, %g1, %g5
  d0:	03 3f ff c0 	sethi  %hi\(0xffff0000\), %g1
  d4:	8a 10 20 01 	mov  1, %g5
  d8:	83 28 70 20 	sllx  %g1, 0x20, %g1
  dc:	8a 11 40 01 	or  %g5, %g1, %g5
  e0:	0b 3f ff c0 	sethi  %hi\(0xffff0000\), %g5
  e4:	82 10 20 01 	mov  1, %g1
  e8:	8a 11 60 01 	or  %g5, 1, %g5
  ec:	83 28 70 20 	sllx  %g1, 0x20, %g1
  f0:	8a 11 40 01 	or  %g5, %g1, %g5
  f4:	0b 3f ff c0 	sethi  %hi\(0xffff0000\), %g5
  f8:	82 10 20 01 	mov  1, %g1
  fc:	83 28 70 20 	sllx  %g1, 0x20, %g1
 100:	8a 11 40 01 	or  %g5, %g1, %g5
 104:	82 10 20 01 	mov  1, %g1
 108:	8a 10 20 01 	mov  1, %g5
 10c:	83 28 70 20 	sllx  %g1, 0x20, %g1
 110:	8a 11 40 01 	or  %g5, %g1, %g5
 114:	05 00 00 00 	sethi  %hi\((0x|)0\), %g2
			114: R_SPARC_HI22	.text
 118:	84 10 a0 00 	mov  %g2, %g2	! 0 <foo>
			118: R_SPARC_LO10	.text
 11c:	07 1d 95 0c 	sethi  %hi\(0x76543000\), %g3
 120:	86 10 e2 10 	or  %g3, 0x210, %g3	! 76543210 <(\*ABS\*|foo)\+0x76543210>
 124:	88 10 20 00 	clr  %g4
 128:	0b 00 00 3f 	sethi  %hi\(0xfc00\), %g5
 12c:	8a 11 63 ff 	or  %g5, 0x3ff, %g5	! ffff <(\*ABS\*|foo)\+0xffff>
 130:	05 00 00 00 	sethi  %hi\((0x|)0\), %g2
			130: R_SPARC_HI22	.text
 134:	84 10 a0 00 	mov  %g2, %g2	! 0 <foo>
			134: R_SPARC_LO10	.text
 138:	85 38 80 00 	signx  %g2
 13c:	07 1d 95 0c 	sethi  %hi\(0x76543000\), %g3
 140:	86 10 e2 10 	or  %g3, 0x210, %g3	! 76543210 <(\*ABS\*|foo)\+0x76543210>
 144:	88 10 20 00 	clr  %g4
 148:	0b 00 00 3f 	sethi  %hi\(0xfc00\), %g5
 14c:	8a 11 63 ff 	or  %g5, 0x3ff, %g5	! ffff <(\*ABS\*|foo)\+0xffff>
 150:	82 10 3f ff 	mov  -1, %g1
 154:	05 1f ff ff 	sethi  %hi\(0x7ffffc00\), %g2
 158:	84 10 a3 ff 	or  %g2, 0x3ff, %g2	! 7fffffff <(\*ABS\*|foo)\+0x7fffffff>
 15c:	07 00 00 3f 	sethi  %hi\(0xfc00\), %g3
 160:	86 18 fc 00 	xor  %g3, -1024, %g3
 164:	88 10 3f ff 	mov  -1, %g4
