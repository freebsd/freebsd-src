#as:
#objdump:  -dr
#name:  loadw_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 89 00 00 	loadw	0x0 <main>:m,r0
   4:	10 89 ff 00 	loadw	0xff <main\+0xff>:m,r1
   8:	30 89 ff 0f 	loadw	0xfff <main\+0xfff>:m,r3
   c:	40 89 34 12 	loadw	0x1234 <main\+0x1234>:m,r4
  10:	50 89 34 12 	loadw	0x1234 <main\+0x1234>:m,r5
  14:	12 00 07 fa 	loadw	0x7a1234 <main\+0x7a1234>:l,r0
  18:	34 12 
  1a:	12 00 1b fa 	loadw	0xba1234 <main\+0xba1234>:l,r1
  1e:	34 12 
  20:	2f 89 ff ff 	loadw	0xfffff <main\+0xfffff>:m,r2
  24:	00 8e 00 00 	loadw	\[r12\]0x0:m,r0
  28:	00 8f 00 00 	loadw	\[r12\]0x0:m,r0
  2c:	10 8e ff 00 	loadw	\[r12\]0xff:m,r1
  30:	10 8f ff 00 	loadw	\[r12\]0xff:m,r1
  34:	30 8e ff 0f 	loadw	\[r12\]0xfff:m,r3
  38:	30 8f ff 0f 	loadw	\[r12\]0xfff:m,r3
  3c:	40 8e 34 12 	loadw	\[r13\]0x1234:m,r4
  40:	40 8f 34 12 	loadw	\[r13\]0x1234:m,r4
  44:	50 8e 34 12 	loadw	\[r13\]0x1234:m,r5
  48:	50 8f 34 12 	loadw	\[r13\]0x1234:m,r5
  4c:	20 8e 67 45 	loadw	\[r12\]0x4567:m,r2
  50:	2a 8f 34 12 	loadw	\[r12\]0xa1234:m,r2
  54:	10 92       	loadw	0x4:s\(r1,r0\),r1
  56:	32 92       	loadw	0x4:s\(r3,r2\),r3
  58:	40 9f 34 12 	loadw	0x1234:m\(r1,r0\),r4
  5c:	52 9f 34 12 	loadw	0x1234:m\(r3,r2\),r5
  60:	12 00 60 da 	loadw	0xa1234:l\(r1,r0\),r6
  64:	34 12 
  66:	18 00 10 df 	loadw	0xffffc:l\(r1,r0\),r1
  6a:	fc ff 
  6c:	18 00 32 df 	loadw	0xffffc:l\(r3,r2\),r3
  70:	fc ff 
  72:	18 00 40 df 	loadw	0xfedcc:l\(r1,r0\),r4
  76:	cc ed 
  78:	18 00 52 df 	loadw	0xfedcc:l\(r3,r2\),r5
  7c:	cc ed 
  7e:	18 00 60 d5 	loadw	0x5edcc:l\(r1,r0\),r6
  82:	cc ed 
  84:	00 90       	loadw	0x0:s\(r1,r0\),r0
  86:	10 90       	loadw	0x0:s\(r1,r0\),r1
  88:	00 9f 0f 00 	loadw	0xf:m\(r1,r0\),r0
  8c:	10 9f 0f 00 	loadw	0xf:m\(r1,r0\),r1
  90:	20 9f 34 12 	loadw	0x1234:m\(r1,r0\),r2
  94:	32 9f cd ab 	loadw	0xabcd:m\(r3,r2\),r3
  98:	43 9f ff af 	loadw	0xafff:m\(r4,r3\),r4
  9c:	12 00 55 da 	loadw	0xa1234:l\(r6,r5\),r5
  a0:	34 12 
  a2:	18 00 00 df 	loadw	0xffff1:l\(r1,r0\),r0
  a6:	f1 ff 
  a8:	18 00 10 df 	loadw	0xffff1:l\(r1,r0\),r1
  ac:	f1 ff 
  ae:	18 00 20 df 	loadw	0xfedcc:l\(r1,r0\),r2
  b2:	cc ed 
  b4:	18 00 32 df 	loadw	0xf5433:l\(r3,r2\),r3
  b8:	33 54 
  ba:	18 00 43 df 	loadw	0xf5001:l\(r4,r3\),r4
  be:	01 50 
  c0:	18 00 55 d5 	loadw	0x5edcc:l\(r6,r5\),r5
  c4:	cc ed 
  c6:	00 9e       	loadw	\[r12\]0x0:s\(r1,r0\),r0
  c8:	18 9e       	loadw	\[r13\]0x0:s\(r1,r0\),r1
  ca:	f0 86 04 12 	loadw	\[r12\]0x234:m\(r1,r0\),r15
  ce:	12 00 38 e1 	loadw	\[r13\]0x1abcd:l\(r1,r0\),r3
  d2:	cd ab 
  d4:	12 00 40 ea 	loadw	\[r12\]0xa1234:l\(r1,r0\),r4
  d8:	34 12 
  da:	12 00 58 eb 	loadw	\[r13\]0xb1234:l\(r1,r0\),r5
  de:	34 12 
  e0:	12 00 68 ef 	loadw	\[r13\]0xfffff:l\(r1,r0\),r6
  e4:	ff ff 
