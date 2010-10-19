#as:
#objdump: -dr
#name: load_stor_insn

.*: +file format .*

Disassembly of section .text:

00000000 <loadb>:
   0:	01 33 00 00 	loadb	0x632 [-_<>+0-9a-z]*, r1
   4:	32 06 
   6:	02 33 08 00 	loadb	0x87632 [-_<>+0-9a-z]*, r2
   a:	32 76 
   c:	03 32 34 12 	loadb	0xffff1234 [-_<>+0-9a-z]*, r3
  10:	95 84       	loadb	0x9\(r5\), r4
  12:	0f 86       	loadb	0x0\(r15\), r6
  14:	e6 87 56 04 	loadb	0x456\(r6\), r7
  18:	e8 8a aa fb 	loadb	0xfbaa\(r8\), r10
  1c:	fd 8c 04 00 	loadb	0x45678\(r13\), r12
  20:	78 56 
  22:	f9 8f a9 fb 	loadb	0xfba9876e\(r9\), r15
  26:	6e 87 
  28:	8e 32 09 f0 	loadb	0x9\(r15\)\+, r14
  2c:	82 32 de df 	loadb	0xfde\(r13\)\+, r2
  30:	cd 33 40 9c 	loadb	0x45\(r9,r12,2\), r13
  34:	45 00 
  36:	ce 33 3f f7 	loadb	0x3ffd6f\(r15,r7,1\), r14
  3a:	6f fd 

0000003c <loadw>:
  3c:	11 33 00 00 	loadw	0x19a [-_<>+0-9a-z]*, r1
  40:	9a 01 
  42:	12 33 01 00 	loadw	0x15650 [-_<>+0-9a-z]*, r2
  46:	50 56 
  48:	13 32 06 00 	loadw	0xffff0006 [-_<>+0-9a-z]*, r3
  4c:	1f 94       	loadw	0x2\(r15\), r4
  4e:	0f 96       	loadw	0x0\(r15\), r6
  50:	e6 97 2e 01 	loadw	0x12e\(r6\), r7
  54:	e8 9a 01 f8 	loadw	0xf801\(r8\), r10
  58:	fd 9c 06 00 	loadw	0x6f855\(r13\), r12
  5c:	55 f8 
  5e:	f9 9f 00 ff 	loadw	0xff000000\(r9\), r15
  62:	00 00 
  64:	9e 32 08 20 	loadw	0x8\(r2\)\+, r14
  68:	92 32 cc df 	loadw	0xfcc\(r13\)\+, r2
  6c:	dd 33 80 9c 	loadw	0x25\(r9,r12,4\), r13
  70:	25 00 
  72:	de 33 ff f7 	loadw	0x3f99a9\(r15,r7,8\), r14
  76:	a9 99 

00000078 <loadd>:
  78:	21 33 00 00 	loadd	0xfff1 [-_<>+0-9a-z]*, r1
  7c:	f1 ff 
  7e:	22 33 ef ff 	loadd	0xffefffef [-_<>+0-9a-z]*, r2
  82:	ef ff 
  84:	23 32 34 12 	loadd	0xffff1234 [-_<>+0-9a-z]*, r3
  88:	e0 a4 0a 00 	loadd	0xa\(r0\), r4
  8c:	0f a6       	loadd	0x0\(r15\), r6
  8e:	e6 a7 00 01 	loadd	0x100\(r6\), r7
  92:	e8 aa 00 ff 	loadd	0xff00\(r8\), r10
  96:	fd ac 01 00 	loadd	0x12000\(r13\), r12
  9a:	00 20 
  9c:	f9 af ce ff 	loadd	0xffce0000\(r9\), r15
  a0:	00 00 
  a2:	ae 32 07 f0 	loadd	0x7\(r15\)\+, r14
  a6:	a2 32 ce ef 	loadd	0xfce\(r14\)\+, r2
  aa:	ed 33 40 9c 	loadd	0x2d\(r9,r12,2\), r13
  ae:	2d 00 
  b0:	ee 33 3f f7 	loadd	0x3ffe51\(r15,r7,1\), r14
  b4:	51 fe 

000000b6 <storb>:
  b6:	01 35 00 00 	storb	r1, 0x632 [-_<>+0-9a-z]*
  ba:	32 06 
  bc:	02 35 08 00 	storb	r2, 0x87632 [-_<>+0-9a-z]*
  c0:	32 76 
  c2:	03 34 34 12 	storb	r3, 0xffff1234 [-_<>+0-9a-z]*
  c6:	95 c4       	storb	r4, 0x9\(r5\)
  c8:	0f c6       	storb	r6, 0x0\(r15\)
  ca:	e6 c7 56 04 	storb	r7, 0x456\(r6\)
  ce:	e8 ca aa fb 	storb	r10, 0xfbaa\(r8\)
  d2:	fd cc 04 00 	storb	r12, 0x45678\(r13\)
  d6:	78 56 
  d8:	f9 cf a9 fb 	storb	r15, 0xfba9876e\(r9\)
  dc:	6e 87 
  de:	8e 34 09 f0 	storb	r14, 0x9\(r15\)\+
  e2:	82 34 de df 	storb	r2, 0xfde\(r13\)\+
  e6:	cd 35 40 9c 	storb	r13, 0x45\(r9,r12,2\)
  ea:	45 00 
  ec:	ce 35 3f f7 	storb	r14, 0x3ffd6f\(r15,r7,1\)
  f0:	6f fd 
  f2:	45 36 09 40 	storb	\$0x5, 0x9\(r4\)
  f6:	4f 37 ff 3f 	storb	\$0xf, 0xffff013\(r3\)
  fa:	13 f0 

000000fc <storw>:
  fc:	11 35 00 00 	storw	r1, 0x19a [-_<>+0-9a-z]*
 100:	9a 01 
 102:	12 35 01 00 	storw	r2, 0x15650 [-_<>+0-9a-z]*
 106:	50 56 
 108:	13 34 06 00 	storw	r3, 0xffff0006 [-_<>+0-9a-z]*
 10c:	1f d4       	storw	r4, 0x2\(r15\)
 10e:	0f d6       	storw	r6, 0x0\(r15\)
 110:	e6 d7 2e 01 	storw	r7, 0x12e\(r6\)
 114:	e8 da 01 f8 	storw	r10, 0xf801\(r8\)
 118:	fd dc 06 00 	storw	r12, 0x6f855\(r13\)
 11c:	55 f8 
 11e:	f9 df 00 ff 	storw	r15, 0xff000000\(r9\)
 122:	00 00 
 124:	9e 34 08 20 	storw	r14, 0x8\(r2\)\+
 128:	92 34 cc df 	storw	r2, 0xfcc\(r13\)\+
 12c:	dd 35 80 9c 	storw	r13, 0x25\(r9,r12,4\)
 130:	25 00 
 132:	de 35 ff f7 	storw	r14, 0x3f99a9\(r15,r7,8\)
 136:	a9 99 
 138:	11 37 00 00 	storw	\$0x1, 0x632 [-_<>+0-9a-z]*
 13c:	32 06 
 13e:	17 37 08 00 	storw	\$0x7, 0x87632 [-_<>+0-9a-z]*
 142:	32 76 

00000144 <stord>:
 144:	21 35 00 00 	stord	r1, 0xfff1 [-_<>+0-9a-z]*
 148:	f1 ff 
 14a:	22 35 ef ff 	stord	r2, 0xffefffef [-_<>+0-9a-z]*
 14e:	ef ff 
 150:	23 34 01 00 	stord	r3, 0xffff0001 [-_<>+0-9a-z]*
 154:	e0 e4 0a 00 	stord	r4, 0xa\(r0\)
 158:	0f e6       	stord	r6, 0x0\(r15\)
 15a:	e6 e7 00 01 	stord	r7, 0x100\(r6\)
 15e:	e8 ea 00 ff 	stord	r10, 0xff00\(r8\)
 162:	fd ec 01 00 	stord	r12, 0x12000\(r13\)
 166:	00 20 
 168:	f9 ef ce ff 	stord	r15, 0xffce0000\(r9\)
 16c:	00 00 
 16e:	ae 34 07 f0 	stord	r14, 0x7\(r15\)\+
 172:	a2 34 ce ef 	stord	r2, 0xfce\(r14\)\+
 176:	ed 35 40 9c 	stord	r13, 0x2d\(r9,r12,2\)
 17a:	2d 00 
 17c:	ee 35 3f f7 	stord	r14, 0x3ffe51\(r15,r7,1\)
 180:	51 fe 
 182:	af 36 05 a0 	stord	\$0xf, 0x5\(r10\)\+
 186:	a0 36 e4 bf 	stord	\$0x0, 0xfe4\(r11\)\+

