#as:
#objdump:  -dr
#name:  loadd_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 87 00 00 	loadd	0x0 <main>:m,\(r1,r0\)
   4:	00 87 ff 00 	loadd	0xff <main\+0xff>:m,\(r1,r0\)
   8:	20 87 ff 0f 	loadd	0xfff <main\+0xfff>:m,\(r3,r2\)
   c:	30 87 34 12 	loadd	0x1234 <main\+0x1234>:m,\(r4,r3\)
  10:	40 87 34 12 	loadd	0x1234 <main\+0x1234>:m,\(r5,r4\)
  14:	12 00 07 ba 	loadd	0x7a1234 <main\+0x7a1234>:l,\(r1,r0\)
  18:	34 12 
  1a:	12 00 0b ba 	loadd	0xba1234 <main\+0xba1234>:l,\(r1,r0\)
  1e:	34 12 
  20:	1f 87 ff ff 	loadd	0xfffff <main\+0xfffff>:m,\(r2,r1\)
  24:	00 8c 00 00 	loadd	\[r12\]0x0:m,\(r1,r0\)
  28:	00 8d 00 00 	loadd	\[r12\]0x0:m,\(r1,r0\)
  2c:	00 8c ff 00 	loadd	\[r12\]0xff:m,\(r1,r0\)
  30:	00 8d ff 00 	loadd	\[r12\]0xff:m,\(r1,r0\)
  34:	20 8c ff 0f 	loadd	\[r12\]0xfff:m,\(r3,r2\)
  38:	20 8d ff 0f 	loadd	\[r12\]0xfff:m,\(r3,r2\)
  3c:	30 8c 34 12 	loadd	\[r12\]0x1234:m,\(r4,r3\)
  40:	30 8d 34 12 	loadd	\[r12\]0x1234:m,\(r4,r3\)
  44:	40 8c 34 12 	loadd	\[r13\]0x1234:m,\(r5,r4\)
  48:	40 8d 34 12 	loadd	\[r13\]0x1234:m,\(r5,r4\)
  4c:	10 8c 67 45 	loadd	\[r12\]0x4567:m,\(r2,r1\)
  50:	1a 8d 34 12 	loadd	\[r12\]0xa1234:m,\(r2,r1\)
  54:	10 a2       	loadd	0x4:s\(r1,r0\),\(r2,r1\)
  56:	22 a2       	loadd	0x4:s\(r3,r2\),\(r3,r2\)
  58:	30 af 34 12 	loadd	0x1234:m\(r1,r0\),\(r4,r3\)
  5c:	42 af 34 12 	loadd	0x1234:m\(r3,r2\),\(r5,r4\)
  60:	12 00 50 9a 	loadd	0xa1234:l\(r1,r0\),\(r6,r5\)
  64:	34 12 
  66:	18 00 10 9f 	loadd	0xffffc:l\(r1,r0\),\(r2,r1\)
  6a:	fc ff 
  6c:	18 00 22 9f 	loadd	0xffffc:l\(r3,r2\),\(r3,r2\)
  70:	fc ff 
  72:	18 00 30 9f 	loadd	0xfedcc:l\(r1,r0\),\(r4,r3\)
  76:	cc ed 
  78:	18 00 42 9f 	loadd	0xfedcc:l\(r3,r2\),\(r5,r4\)
  7c:	cc ed 
  7e:	18 00 50 95 	loadd	0x5edcc:l\(r1,r0\),\(r6,r5\)
  82:	cc ed 
  84:	00 a0       	loadd	0x0:s\(r1,r0\),\(r1,r0\)
  86:	00 a0       	loadd	0x0:s\(r1,r0\),\(r1,r0\)
  88:	00 af 0f 00 	loadd	0xf:m\(r1,r0\),\(r1,r0\)
  8c:	00 af 0f 00 	loadd	0xf:m\(r1,r0\),\(r1,r0\)
  90:	10 af 34 12 	loadd	0x1234:m\(r1,r0\),\(r2,r1\)
  94:	22 af cd ab 	loadd	0xabcd:m\(r3,r2\),\(r3,r2\)
  98:	33 af ff af 	loadd	0xafff:m\(r4,r3\),\(r4,r3\)
  9c:	12 00 65 9a 	loadd	0xa1234:l\(r6,r5\),\(r7,r6\)
  a0:	34 12 
  a2:	18 00 00 9f 	loadd	0xffff1:l\(r1,r0\),\(r1,r0\)
  a6:	f1 ff 
  a8:	18 00 00 9f 	loadd	0xffff1:l\(r1,r0\),\(r1,r0\)
  ac:	f1 ff 
  ae:	18 00 10 9f 	loadd	0xfedcc:l\(r1,r0\),\(r2,r1\)
  b2:	cc ed 
  b4:	18 00 22 9f 	loadd	0xf5433:l\(r3,r2\),\(r3,r2\)
  b8:	33 54 
  ba:	18 00 43 9f 	loadd	0xf5001:l\(r4,r3\),\(r5,r4\)
  be:	01 50 
  c0:	18 00 45 95 	loadd	0x5edcc:l\(r6,r5\),\(r5,r4\)
  c4:	cc ed 
  c6:	00 ae       	loadd	\[r12\]0x0:s\(r1,r0\),\(r1,r0\)
  c8:	08 ae       	loadd	\[r13\]0x0:s\(r1,r0\),\(r1,r0\)
  ca:	b0 86 04 12 	loadd	\[r12\]0x234:m\(r1,r0\),\(r12,r11\)
  ce:	12 00 28 a1 	loadd	\[r13\]0x1abcd:l\(r1,r0\),\(r3,r2\)
  d2:	cd ab 
  d4:	12 00 20 aa 	loadd	\[r12\]0xa1234:l\(r1,r0\),\(r3,r2\)
  d8:	34 12 
  da:	12 00 38 ab 	loadd	\[r13\]0xb1234:l\(r1,r0\),\(r4,r3\)
  de:	34 12 
  e0:	12 00 48 af 	loadd	\[r13\]0xfffff:l\(r1,r0\),\(r5,r4\)
  e4:	ff ff 
