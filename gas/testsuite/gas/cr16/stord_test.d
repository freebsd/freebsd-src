#as:
#objdump:  -dr
#name:  stord_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 c7 00 00 	stord	\(r1,r0\),0x0 <main>:m
   4:	00 c7 ff 00 	stord	\(r1,r0\),0xff <main\+0xff>:m
   8:	20 c7 ff 0f 	stord	\(r3,r2\),0xfff <main\+0xfff>:m
   c:	30 c7 34 12 	stord	\(r4,r3\),0x1234 <main\+0x1234>:m
  10:	40 c7 34 12 	stord	\(r5,r4\),0x1234 <main\+0x1234>:m
  14:	13 00 07 ba 	stord	\(r1,r0\),0x7a1234 <main\+0x7a1234>:l
  18:	34 12 
  1a:	13 00 0b ba 	stord	\(r1,r0\),0xba1234 <main\+0xba1234>:l
  1e:	34 12 
  20:	13 00 1f bf 	stord	\(r2,r1\),0xffffff <main\+0xffffff>:l
  24:	ff ff 
  26:	00 cc 00 00 	stord	\(r1,r0\),\[r12\]0x0:m
  2a:	00 cd 00 00 	stord	\(r1,r0\),\[r12\]0x0:m
  2e:	00 cc ff 00 	stord	\(r1,r0\),\[r12\]0xff:m
  32:	00 cd ff 00 	stord	\(r1,r0\),\[r12\]0xff:m
  36:	20 cc ff 0f 	stord	\(r3,r2\),\[r12\]0xfff:m
  3a:	20 cd ff 0f 	stord	\(r3,r2\),\[r12\]0xfff:m
  3e:	30 cc 34 12 	stord	\(r4,r3\),\[r12\]0x1234:m
  42:	30 cd 34 12 	stord	\(r4,r3\),\[r12\]0x1234:m
  46:	40 cc 34 12 	stord	\(r5,r4\),\[r13\]0x1234:m
  4a:	40 cd 34 12 	stord	\(r5,r4\),\[r13\]0x1234:m
  4e:	10 cc 67 45 	stord	\(r2,r1\),\[r12\]0x4567:m
  52:	1a cd 34 12 	stord	\(r2,r1\),\[r12\]0xa1234:m
  56:	10 e2       	stord	\(r2,r1\),0x4:s\(r1,r0\)
  58:	22 e2       	stord	\(r3,r2\),0x4:s\(r3,r2\)
  5a:	30 ef 34 12 	stord	\(r4,r3\),0x1234:m\(r1,r0\)
  5e:	42 ef 34 12 	stord	\(r5,r4\),0x1234:m\(r3,r2\)
  62:	13 00 50 9a 	stord	\(r6,r5\),0xa1234:l\(r1,r0\)
  66:	34 12 
  68:	19 00 10 9f 	stord	\(r2,r1\),0xffffc:l\(r1,r0\)
  6c:	fc ff 
  6e:	19 00 22 9f 	stord	\(r3,r2\),0xffffc:l\(r3,r2\)
  72:	fc ff 
  74:	19 00 30 9f 	stord	\(r4,r3\),0xfedcc:l\(r1,r0\)
  78:	cc ed 
  7a:	19 00 42 9f 	stord	\(r5,r4\),0xfedcc:l\(r3,r2\)
  7e:	cc ed 
  80:	19 00 50 95 	stord	\(r6,r5\),0x5edcc:l\(r1,r0\)
  84:	cc ed 
  86:	00 e0       	stord	\(r1,r0\),0x0:s\(r1,r0\)
  88:	00 e0       	stord	\(r1,r0\),0x0:s\(r1,r0\)
  8a:	00 ef 0f 00 	stord	\(r1,r0\),0xf:m\(r1,r0\)
  8e:	00 ef 0f 00 	stord	\(r1,r0\),0xf:m\(r1,r0\)
  92:	10 ef 34 12 	stord	\(r2,r1\),0x1234:m\(r1,r0\)
  96:	22 ef cd ab 	stord	\(r3,r2\),0xabcd:m\(r3,r2\)
  9a:	33 ef ff af 	stord	\(r4,r3\),0xafff:m\(r4,r3\)
  9e:	13 00 65 9a 	stord	\(r7,r6\),0xa1234:l\(r6,r5\)
  a2:	34 12 
  a4:	19 00 00 9f 	stord	\(r1,r0\),0xffff1:l\(r1,r0\)
  a8:	f1 ff 
  aa:	19 00 00 9f 	stord	\(r1,r0\),0xffff1:l\(r1,r0\)
  ae:	f1 ff 
  b0:	19 00 10 9f 	stord	\(r2,r1\),0xfedcc:l\(r1,r0\)
  b4:	cc ed 
  b6:	19 00 22 9f 	stord	\(r3,r2\),0xf5433:l\(r3,r2\)
  ba:	33 54 
  bc:	19 00 43 9f 	stord	\(r5,r4\),0xf5001:l\(r4,r3\)
  c0:	01 50 
  c2:	19 00 45 95 	stord	\(r5,r4\),0x5edcc:l\(r6,r5\)
  c6:	cc ed 
  c8:	00 ee       	stord	\(r1,r0\),\[r12\]0x0:s\(r1,r0\)
  ca:	08 ee       	stord	\(r1,r0\),\[r13\]0x0:s\(r1,r0\)
  cc:	b0 c6 04 12 	stord	\(r12,r11\),\[r12\]0x234:m\(r1,r0\)
  d0:	13 00 28 a1 	stord	\(r3,r2\),\[r13\]0x1abcd:l\(r1,r0\)
  d4:	cd ab 
  d6:	13 00 20 aa 	stord	\(r3,r2\),\[r12\]0xa1234:l\(r1,r0\)
  da:	34 12 
  dc:	13 00 38 ab 	stord	\(r4,r3\),\[r13\]0xb1234:l\(r1,r0\)
  e0:	34 12 
  e2:	13 00 48 af 	stord	\(r5,r4\),\[r13\]0xfffff:l\(r1,r0\)
  e6:	ff ff 
