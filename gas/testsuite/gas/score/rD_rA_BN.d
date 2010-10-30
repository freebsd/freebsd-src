#as:
#objdump: -d
#source: rD_rA_BN.s

.*: +file format .*

Disassembly of section \.text:

00000000 <\.text>:
   0:	6014      	bitclr!		r0, 0x2
   2:	6014      	bitclr!		r0, 0x2
   4:	6f24      	bitclr!		r15, 0x4
   6:	6f24      	bitclr!		r15, 0x4
   8:	6f0c      	bitclr!		r15, 0x1
   a:	6f0c      	bitclr!		r15, 0x1
   c:	6f1c      	bitclr!		r15, 0x3
   e:	6f1c      	bitclr!		r15, 0x3
  10:	681c      	bitclr!		r8, 0x3
  12:	681c      	bitclr!		r8, 0x3
  14:	81ef8429 	bitclr.c	r15, r15, 0x1
  18:	83579029 	bitclr.c	r26, r23, 0x4
  1c:	0000      	nop!
  1e:	0000      	nop!
  20:	6015      	bitset!		r0, 0x2
  22:	6015      	bitset!		r0, 0x2
  24:	6f25      	bitset!		r15, 0x4
  26:	6f25      	bitset!		r15, 0x4
  28:	6f0d      	bitset!		r15, 0x1
  2a:	6f0d      	bitset!		r15, 0x1
  2c:	6f1d      	bitset!		r15, 0x3
  2e:	6f1d      	bitset!		r15, 0x3
  30:	681d      	bitset!		r8, 0x3
  32:	681d      	bitset!		r8, 0x3
  34:	81ef842b 	bitset.c	r15, r15, 0x1
  38:	8357902b 	bitset.c	r26, r23, 0x4
  3c:	0000      	nop!
  3e:	0000      	nop!
  40:	6017      	bittgl!		r0, 0x2
  42:	6017      	bittgl!		r0, 0x2
  44:	6f27      	bittgl!		r15, 0x4
  46:	6f27      	bittgl!		r15, 0x4
  48:	6f0f      	bittgl!		r15, 0x1
  4a:	6f0f      	bittgl!		r15, 0x1
  4c:	6f1f      	bittgl!		r15, 0x3
  4e:	6f1f      	bittgl!		r15, 0x3
  50:	681f      	bittgl!		r8, 0x3
  52:	681f      	bittgl!		r8, 0x3
  54:	81ef842f 	bittgl.c	r15, r15, 0x1
  58:	8357902f 	bittgl.c	r26, r23, 0x4
  5c:	0000      	nop!
  5e:	0000      	nop!
  60:	6011      	slli!		r0, 2
  62:	6011      	slli!		r0, 2
  64:	6f21      	slli!		r15, 4
  66:	6f21      	slli!		r15, 4
  68:	6f09      	slli!		r15, 1
  6a:	6f09      	slli!		r15, 1
  6c:	6f19      	slli!		r15, 3
  6e:	6f19      	slli!		r15, 3
  70:	6819      	slli!		r8, 3
  72:	6819      	slli!		r8, 3
  74:	81ef8471 	slli.c		r15, r15, 1
  78:	83579071 	slli.c		r26, r23, 4
  7c:	0000      	nop!
  7e:	0000      	nop!
  80:	6013      	srli!		r0, 2
  82:	6013      	srli!		r0, 2
  84:	6f23      	srli!		r15, 4
  86:	6f23      	srli!		r15, 4
  88:	6f0b      	srli!		r15, 1
  8a:	6f0b      	srli!		r15, 1
  8c:	6f1b      	srli!		r15, 3
  8e:	6f1b      	srli!		r15, 3
  90:	681b      	srli!		r8, 3
  92:	681b      	srli!		r8, 3
  94:	81ef8475 	srli.c		r15, r15, 1
  98:	83579075 	srli.c		r26, r23, 4
  9c:	0000      	nop!
  9e:	0000      	nop!
  a0:	80008829 	bitclr.c	r0, r0, 0x2
  a4:	82958829 	bitclr.c	r20, r21, 0x2
  a8:	81ef9029 	bitclr.c	r15, r15, 0x4
  ac:	83359029 	bitclr.c	r25, r21, 0x4
  b0:	81ef8429 	bitclr.c	r15, r15, 0x1
  b4:	83368429 	bitclr.c	r25, r22, 0x1
  b8:	681c      	bitclr!		r8, 0x3
  ba:	681c      	bitclr!		r8, 0x3
  bc:	6624      	bitclr!		r6, 0x4
  be:	6624      	bitclr!		r6, 0x4
  c0:	6914      	bitclr!		r9, 0x2
  c2:	6914      	bitclr!		r9, 0x2
	...
  d0:	8000882b 	bitset.c	r0, r0, 0x2
  d4:	8295882b 	bitset.c	r20, r21, 0x2
  d8:	81ef902b 	bitset.c	r15, r15, 0x4
  dc:	8335902b 	bitset.c	r25, r21, 0x4
  e0:	81ef842b 	bitset.c	r15, r15, 0x1
  e4:	8336842b 	bitset.c	r25, r22, 0x1
  e8:	681d      	bitset!		r8, 0x3
  ea:	681d      	bitset!		r8, 0x3
  ec:	6625      	bitset!		r6, 0x4
  ee:	6625      	bitset!		r6, 0x4
  f0:	6915      	bitset!		r9, 0x2
  f2:	6915      	bitset!		r9, 0x2
	...
 100:	8000882f 	bittgl.c	r0, r0, 0x2
 104:	8295882f 	bittgl.c	r20, r21, 0x2
 108:	81ef902f 	bittgl.c	r15, r15, 0x4
 10c:	8335902f 	bittgl.c	r25, r21, 0x4
 110:	81ef842f 	bittgl.c	r15, r15, 0x1
 114:	8336842f 	bittgl.c	r25, r22, 0x1
 118:	681f      	bittgl!		r8, 0x3
 11a:	681f      	bittgl!		r8, 0x3
 11c:	6627      	bittgl!		r6, 0x4
 11e:	6627      	bittgl!		r6, 0x4
 120:	6917      	bittgl!		r9, 0x2
 122:	6917      	bittgl!		r9, 0x2
	...
 130:	80008871 	slli.c		r0, r0, 2
 134:	82958871 	slli.c		r20, r21, 2
 138:	81ef9071 	slli.c		r15, r15, 4
 13c:	83359071 	slli.c		r25, r21, 4
 140:	81ef8471 	slli.c		r15, r15, 1
 144:	83368471 	slli.c		r25, r22, 1
 148:	6819      	slli!		r8, 3
 14a:	6819      	slli!		r8, 3
 14c:	6621      	slli!		r6, 4
 14e:	6621      	slli!		r6, 4
 150:	6911      	slli!		r9, 2
 152:	6911      	slli!		r9, 2
	...
 160:	80008875 	srli.c		r0, r0, 2
 164:	82958875 	srli.c		r20, r21, 2
 168:	81ef9075 	srli.c		r15, r15, 4
 16c:	83359075 	srli.c		r25, r21, 4
 170:	81ef8475 	srli.c		r15, r15, 1
 174:	83368475 	srli.c		r25, r22, 1
 178:	681b      	srli!		r8, 3
 17a:	681b      	srli!		r8, 3
 17c:	6623      	srli!		r6, 4
 17e:	6623      	srli!		r6, 4
 180:	6913      	srli!		r9, 2
 182:	6913      	srli!		r9, 2
#pass
