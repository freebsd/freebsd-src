#as:
#objdump:  -dr
#name:  mul_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	f1 64       	mulb	\$0xf:s,r1
   2:	b2 64 ff 00 	mulb	\$0xff:m,r2
   6:	b1 64 ff 0f 	mulb	\$0xfff:m,r1
   a:	b1 64 14 00 	mulb	\$0x14:m,r1
   e:	a2 64       	mulb	\$0xa:s,r2
  10:	12 65       	mulb	r1,r2
  12:	23 65       	mulb	r2,r3
  14:	34 65       	mulb	r3,r4
  16:	56 65       	mulb	r5,r6
  18:	67 65       	mulb	r6,r7
  1a:	78 65       	mulb	r7,r8
  1c:	f1 66       	mulw	\$0xf:s,r1
  1e:	b2 66 ff 00 	mulw	\$0xff:m,r2
  22:	b1 66 ff 0f 	mulw	\$0xfff:m,r1
  26:	b1 66 14 00 	mulw	\$0x14:m,r1
  2a:	a2 66       	mulw	\$0xa:s,r2
  2c:	12 67       	mulw	r1,r2
  2e:	23 67       	mulw	r2,r3
  30:	34 67       	mulw	r3,r4
  32:	56 67       	mulw	r5,r6
  34:	67 67       	mulw	r6,r7
  36:	78 67       	mulw	r7,r8
  38:	12 0b       	mulsb	r1,r2
  3a:	34 0b       	mulsb	r3,r4
  3c:	56 0b       	mulsb	r5,r6
  3e:	78 0b       	mulsb	r7,r8
  40:	9a 0b       	mulsb	r9,r10
  42:	12 62       	mulsw	r1,\(r3,r2\)
  44:	33 62       	mulsw	r3,\(r4,r3\)
  46:	55 62       	mulsw	r5,\(r6,r5\)
  48:	77 62       	mulsw	r7,\(r8,r7\)
  4a:	98 62       	mulsw	r9,\(r9,r8\)
  4c:	14 00 12 d2 	macqw	r1,r2,\(r3,r2\)
  50:	14 00 45 d4 	macqw	r4,r5,\(r5,r4\)
  54:	14 00 12 e2 	macuw	r1,r2,\(r3,r2\)
  58:	14 00 45 e7 	macuw	r4,r5,\(r8,r7\)
  5c:	14 00 12 f2 	macsw	r1,r2,\(r3,r2\)
  60:	14 00 45 f6 	macsw	r4,r5,\(r7,r6\)
