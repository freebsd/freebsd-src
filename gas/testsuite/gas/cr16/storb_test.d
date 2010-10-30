#as:
#objdump:  -dr
#name:  storb_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 c8 00 00 	storb	r0,0x0 <main>:m
   4:	10 c8 ff 00 	storb	r1,0xff <main\+0xff>:m
   8:	30 c8 ff 0f 	storb	r3,0xfff <main\+0xfff>:m
   c:	40 c8 34 12 	storb	r4,0x1234 <main\+0x1234>:m
  10:	50 c8 34 12 	storb	r5,0x1234 <main\+0x1234>:m
  14:	13 00 07 7a 	storb	r0,0x7a1234 <main\+0x7a1234>:l
  18:	34 12 
  1a:	13 00 1b 7a 	storb	r1,0xba1234 <main\+0xba1234>:l
  1e:	34 12 
  20:	13 00 2f 7f 	storb	r2,0xffffff <main\+0xffffff>:l
  24:	ff ff 
  26:	00 ca 00 00 	storb	r0,\[r12\]0x0:m
  2a:	00 cb 00 00 	storb	r0,\[r12\]0x0:m
  2e:	10 ca ff 00 	storb	r1,\[r12\]0xff:m
  32:	10 cb ff 00 	storb	r1,\[r12\]0xff:m
  36:	30 ca ff 0f 	storb	r3,\[r12\]0xfff:m
  3a:	30 cb ff 0f 	storb	r3,\[r12\]0xfff:m
  3e:	40 ca 34 12 	storb	r4,\[r13\]0x1234:m
  42:	40 cb 34 12 	storb	r4,\[r13\]0x1234:m
  46:	50 ca 34 12 	storb	r5,\[r13\]0x1234:m
  4a:	50 cb 34 12 	storb	r5,\[r13\]0x1234:m
  4e:	20 ca 67 45 	storb	r2,\[r12\]0x4567:m
  52:	2a cb 34 12 	storb	r2,\[r12\]0xa1234:m
  56:	10 f4       	storb	r1,0x4:s\(r1,r0\)
  58:	32 f4       	storb	r3,0x4:s\(r3,r2\)
  5a:	40 ff 34 12 	storb	r4,0x1234:m\(r1,r0\)
  5e:	52 ff 34 12 	storb	r5,0x1234:m\(r3,r2\)
  62:	13 00 60 5a 	storb	r6,0xa1234:l\(r1,r0\)
  66:	34 12 
  68:	19 00 10 5f 	storb	r1,0xffffc:l\(r1,r0\)
  6c:	fc ff 
  6e:	19 00 32 5f 	storb	r3,0xffffc:l\(r3,r2\)
  72:	fc ff 
  74:	19 00 40 5f 	storb	r4,0xfedcc:l\(r1,r0\)
  78:	cc ed 
  7a:	19 00 52 5f 	storb	r5,0xfedcc:l\(r3,r2\)
  7e:	cc ed 
  80:	19 00 60 55 	storb	r6,0x5edcc:l\(r1,r0\)
  84:	cc ed 
  86:	00 f0       	storb	r0,0x0:s\(r1,r0\)
  88:	00 f0       	storb	r0,0x0:s\(r1,r0\)
  8a:	00 ff 0f 00 	storb	r0,0xf:m\(r1,r0\)
  8e:	10 ff 0f 00 	storb	r1,0xf:m\(r1,r0\)
  92:	20 ff 34 12 	storb	r2,0x1234:m\(r1,r0\)
  96:	32 ff cd ab 	storb	r3,0xabcd:m\(r3,r2\)
  9a:	43 ff ff af 	storb	r4,0xafff:m\(r4,r3\)
  9e:	13 00 55 5a 	storb	r5,0xa1234:l\(r6,r5\)
  a2:	34 12 
  a4:	19 00 00 5f 	storb	r0,0xffff1:l\(r1,r0\)
  a8:	f1 ff 
  aa:	19 00 10 5f 	storb	r1,0xffff1:l\(r1,r0\)
  ae:	f1 ff 
  b0:	19 00 20 5f 	storb	r2,0xfedcc:l\(r1,r0\)
  b4:	cc ed 
  b6:	19 00 32 5f 	storb	r3,0xf5433:l\(r3,r2\)
  ba:	33 54 
  bc:	19 00 43 5f 	storb	r4,0xf5001:l\(r4,r3\)
  c0:	01 50 
  c2:	19 00 55 55 	storb	r5,0x5edcc:l\(r6,r5\)
  c6:	cc ed 
  c8:	00 fe       	storb	r0,\[r12\]0x0:s\(r1,r0\)
  ca:	18 fe       	storb	r1,\[r13\]0x0:s\(r1,r0\)
  cc:	70 c6 04 12 	storb	r7,\[r12\]0x234:m\(r1,r0\)
  d0:	13 00 38 61 	storb	r3,\[r13\]0x1abcd:l\(r1,r0\)
  d4:	cd ab 
  d6:	13 00 40 6a 	storb	r4,\[r12\]0xa1234:l\(r1,r0\)
  da:	34 12 
  dc:	13 00 58 6b 	storb	r5,\[r13\]0xb1234:l\(r1,r0\)
  e0:	34 12 
  e2:	13 00 68 6f 	storb	r6,\[r13\]0xfffff:l\(r1,r0\)
  e6:	ff ff 
  e8:	40 81 cd 0b 	storb	\$0x4:s,0xbcd <main\+0xbcd>:m
  ec:	5a 81 cd ab 	storb	\$0x5:s,0xaabcd <main\+0xaabcd>:m
  f0:	12 00 3f 3a 	storb	\$0x3:s,0xfaabcd <main\+0xfaabcd>:l
  f4:	cd ab 
  f6:	50 84 14 00 	storb	\$0x5:s,\[r13\]0x14:m
  fa:	40 85 fc ab 	storb	\$0x4:s,\[r13\]0xabfc:m
  fe:	30 84 34 12 	storb	\$0x3:s,\[r12\]0x1234:m
 102:	30 85 34 12 	storb	\$0x3:s,\[r12\]0x1234:m
 106:	30 84 34 00 	storb	\$0x3:s,\[r12\]0x34:m
 10a:	30 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r1,r0\)
 10e:	31 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r3,r2\)
 112:	36 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r4,r3\)
 116:	32 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r5,r4\)
 11a:	37 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r6,r5\)
 11e:	33 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r7,r6\)
 122:	34 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r9,r8\)
 126:	35 86 3a 4a 	storb	\$0x3:s,\[r12\]0xa7a:m\(r11,r10\)
 12a:	38 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r1,r0\)
 12e:	39 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r3,r2\)
 132:	3e 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r4,r3\)
 136:	3a 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r5,r4\)
 13a:	3f 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r6,r5\)
 13e:	3b 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r7,r6\)
 142:	3c 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r9,r8\)
 146:	3d 86 3a 4a 	storb	\$0x3:s,\[r13\]0xa7a:m\(r11,r10\)
 14a:	3e 86 5a 4b 	storb	\$0x5:s,\[r13\]0xb7a:m\(r4,r3\)
 14e:	37 86 1a 41 	storb	\$0x1:s,\[r12\]0x17a:m\(r6,r5\)
 152:	3f 86 14 01 	storb	\$0x1:s,\[r13\]0x134:m\(r6,r5\)
 156:	12 00 36 2a 	storb	\$0x3:s,\[r12\]0xabcde:l\(r4,r3\)
 15a:	de bc 
 15c:	12 00 5e 20 	storb	\$0x5:s,\[r13\]0xabcd:l\(r4,r3\)
 160:	cd ab 
 162:	12 00 37 20 	storb	\$0x3:s,\[r12\]0xabcd:l\(r6,r5\)
 166:	cd ab 
 168:	12 00 3f 20 	storb	\$0x3:s,\[r13\]0xbcde:l\(r6,r5\)
 16c:	de bc 
 16e:	12 00 52 00 	storb	\$0x5:s,0x0:l\(r2\)
 172:	00 00 
 174:	3c 83 34 00 	storb	\$0x3:s,0x34:m\(r12\)
 178:	3d 83 ab 00 	storb	\$0x3:s,0xab:m\(r13\)
 17c:	12 00 51 00 	storb	\$0x5:s,0xad:l\(r1\)
 180:	ad 00 
 182:	12 00 52 00 	storb	\$0x5:s,0xcd:l\(r2\)
 186:	cd 00 
 188:	12 00 50 00 	storb	\$0x5:s,0xfff:l\(r0\)
 18c:	ff 0f 
 18e:	12 00 34 00 	storb	\$0x3:s,0xbcd:l\(r4\)
 192:	cd 0b 
 194:	3c 83 ff 0f 	storb	\$0x3:s,0xfff:m\(r12\)
 198:	3d 83 ff 0f 	storb	\$0x3:s,0xfff:m\(r13\)
 19c:	3d 83 ff ff 	storb	\$0x3:s,0xffff:m\(r13\)
 1a0:	3c 83 43 23 	storb	\$0x3:s,0x2343:m\(r12\)
 1a4:	12 00 32 01 	storb	\$0x3:s,0x2345:l\(r2\)
 1a8:	45 23 
 1aa:	12 00 38 04 	storb	\$0x3:s,0xabcd:l\(r8\)
 1ae:	cd ab 
 1b0:	12 00 3d 1f 	storb	\$0x3:s,0xfabcd:l\(r13\)
 1b4:	cd ab 
 1b6:	12 00 38 0f 	storb	\$0x3:s,0xabcd:l\(r8\)
 1ba:	cd ab 
 1bc:	12 00 39 0f 	storb	\$0x3:s,0xabcd:l\(r9\)
 1c0:	cd ab 
 1c2:	12 00 39 04 	storb	\$0x3:s,0xabcd:l\(r9\)
 1c6:	cd ab 
 1c8:	31 82       	storb	\$0x3:s,0x0:s\(r2,r1\)
 1ca:	51 83 01 00 	storb	\$0x5:s,0x1:m\(r2,r1\)
 1ce:	41 83 34 12 	storb	\$0x4:s,0x1234:m\(r2,r1\)
 1d2:	31 83 34 12 	storb	\$0x3:s,0x1234:m\(r2,r1\)
 1d6:	12 00 31 11 	storb	\$0x3:s,0x12345:l\(r2,r1\)
 1da:	45 23 
 1dc:	31 83 23 01 	storb	\$0x3:s,0x123:m\(r2,r1\)
 1e0:	12 00 31 11 	storb	\$0x3:s,0x12345:l\(r2,r1\)
 1e4:	45 23 
