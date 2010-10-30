#as:
#objdump:  -dr
#name:  loadd_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 88 00 00 	loadb	0x0 <main>:m,r0
   4:	10 88 ff 00 	loadb	0xff <main\+0xff>:m,r1
   8:	30 88 ff 0f 	loadb	0xfff <main\+0xfff>:m,r3
   c:	40 88 34 12 	loadb	0x1234 <main\+0x1234>:m,r4
  10:	50 88 34 12 	loadb	0x1234 <main\+0x1234>:m,r5
  14:	12 00 07 7a 	loadb	0x7a1234 <main\+0x7a1234>:l,r0
  18:	34 12 
  1a:	12 00 1b 7a 	loadb	0xba1234 <main\+0xba1234>:l,r1
  1e:	34 12 
  20:	2f 88 ff ff 	loadb	0xfffff <main\+0xfffff>:m,r2
  24:	00 8a 00 00 	loadb	\[r12\]0x0:m,r0
  28:	00 8b 00 00 	loadb	\[r12\]0x0:m,r0
  2c:	10 8a ff 00 	loadb	\[r12\]0xff:m,r1
  30:	10 8b ff 00 	loadb	\[r12\]0xff:m,r1
  34:	30 8a ff 0f 	loadb	\[r12\]0xfff:m,r3
  38:	30 8b ff 0f 	loadb	\[r12\]0xfff:m,r3
  3c:	40 8a 34 12 	loadb	\[r13\]0x1234:m,r4
  40:	40 8b 34 12 	loadb	\[r13\]0x1234:m,r4
  44:	50 8a 34 12 	loadb	\[r13\]0x1234:m,r5
  48:	50 8b 34 12 	loadb	\[r13\]0x1234:m,r5
  4c:	20 8a 67 45 	loadb	\[r12\]0x4567:m,r2
  50:	2a 8b 34 12 	loadb	\[r12\]0xa1234:m,r2
  54:	10 b4       	loadb	0x4:s\(r1,r0\),r1
  56:	32 b4       	loadb	0x4:s\(r3,r2\),r3
  58:	40 bf 34 12 	loadb	0x1234:m\(r1,r0\),r4
  5c:	52 bf 34 12 	loadb	0x1234:m\(r3,r2\),r5
  60:	12 00 60 5a 	loadb	0xa1234:l\(r1,r0\),r6
  64:	34 12 
  66:	18 00 10 5f 	loadb	0xffffc:l\(r1,r0\),r1
  6a:	fc ff 
  6c:	18 00 32 5f 	loadb	0xffffc:l\(r3,r2\),r3
  70:	fc ff 
  72:	18 00 40 5f 	loadb	0xfedcc:l\(r1,r0\),r4
  76:	cc ed 
  78:	18 00 52 5f 	loadb	0xfedcc:l\(r3,r2\),r5
  7c:	cc ed 
  7e:	18 00 60 55 	loadb	0x5edcc:l\(r1,r0\),r6
  82:	cc ed 
  84:	00 b0       	loadb	0x0:s\(r1,r0\),r0
  86:	10 b0       	loadb	0x0:s\(r1,r0\),r1
  88:	00 bf 0f 00 	loadb	0xf:m\(r1,r0\),r0
  8c:	10 bf 0f 00 	loadb	0xf:m\(r1,r0\),r1
  90:	20 bf 34 12 	loadb	0x1234:m\(r1,r0\),r2
  94:	32 bf cd ab 	loadb	0xabcd:m\(r3,r2\),r3
  98:	43 bf ff af 	loadb	0xafff:m\(r4,r3\),r4
  9c:	12 00 55 5a 	loadb	0xa1234:l\(r6,r5\),r5
  a0:	34 12 
  a2:	18 00 00 5f 	loadb	0xffff1:l\(r1,r0\),r0
  a6:	f1 ff 
  a8:	18 00 10 5f 	loadb	0xffff1:l\(r1,r0\),r1
  ac:	f1 ff 
  ae:	18 00 20 5f 	loadb	0xfedcc:l\(r1,r0\),r2
  b2:	cc ed 
  b4:	18 00 32 5f 	loadb	0xf5433:l\(r3,r2\),r3
  b8:	33 54 
  ba:	18 00 43 5f 	loadb	0xf5001:l\(r4,r3\),r4
  be:	01 50 
  c0:	18 00 55 55 	loadb	0x5edcc:l\(r6,r5\),r5
  c4:	cc ed 
  c6:	00 be       	loadb	\[r12\]0x0:s\(r1,r0\),r0
  c8:	18 be       	loadb	\[r13\]0x0:s\(r1,r0\),r1
  ca:	70 86 04 12 	loadb	\[r12\]0x234:m\(r1,r0\),r7
  ce:	12 00 38 61 	loadb	\[r13\]0x1abcd:l\(r1,r0\),r3
  d2:	cd ab 
  d4:	12 00 40 6a 	loadb	\[r12\]0xa1234:l\(r1,r0\),r4
  d8:	34 12 
  da:	12 00 58 6b 	loadb	\[r13\]0xb1234:l\(r1,r0\),r5
  de:	34 12 
  e0:	12 00 68 6f 	loadb	\[r13\]0xfffff:l\(r1,r0\),r6
  e4:	ff ff 
