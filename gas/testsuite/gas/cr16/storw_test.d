#as:
#objdump:  -dr
#name:  storw_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	00 c9 00 00 	storw	r0,0x0 <main>:m
   4:	10 c9 ff 00 	storw	r1,0xff <main\+0xff>:m
   8:	30 c9 ff 0f 	storw	r3,0xfff <main\+0xfff>:m
   c:	40 c9 34 12 	storw	r4,0x1234 <main\+0x1234>:m
  10:	50 c9 34 12 	storw	r5,0x1234 <main\+0x1234>:m
  14:	13 00 07 fa 	storw	r0,0x7a1234 <main\+0x7a1234>:l
  18:	34 12 
  1a:	13 00 1b fa 	storw	r1,0xba1234 <main\+0xba1234>:l
  1e:	34 12 
  20:	13 00 2f ff 	storw	r2,0xffffff <main\+0xffffff>:l
  24:	ff ff 
  26:	00 ce 00 00 	storw	r0,\[r12\]0x0:m
  2a:	00 cf 00 00 	storw	r0,\[r12\]0x0:m
  2e:	10 ce ff 00 	storw	r1,\[r12\]0xff:m
  32:	10 cf ff 00 	storw	r1,\[r12\]0xff:m
  36:	30 ce ff 0f 	storw	r3,\[r12\]0xfff:m
  3a:	30 cf ff 0f 	storw	r3,\[r12\]0xfff:m
  3e:	40 ce 34 12 	storw	r4,\[r13\]0x1234:m
  42:	40 cf 34 12 	storw	r4,\[r13\]0x1234:m
  46:	50 ce 34 12 	storw	r5,\[r13\]0x1234:m
  4a:	50 cf 34 12 	storw	r5,\[r13\]0x1234:m
  4e:	20 ce 67 45 	storw	r2,\[r12\]0x4567:m
  52:	2a cf 34 12 	storw	r2,\[r12\]0xa1234:m
  56:	10 d2       	storw	r1,0x4:s\(r1,r0\)
  58:	32 d2       	storw	r3,0x4:s\(r3,r2\)
  5a:	40 df 34 12 	storw	r4,0x1234:m\(r1,r0\)
  5e:	52 df 34 12 	storw	r5,0x1234:m\(r3,r2\)
  62:	13 00 60 da 	storw	r6,0xa1234:l\(r1,r0\)
  66:	34 12 
  68:	19 00 10 df 	storw	r1,0xffffc:l\(r1,r0\)
  6c:	fc ff 
  6e:	19 00 32 df 	storw	r3,0xffffc:l\(r3,r2\)
  72:	fc ff 
  74:	19 00 40 df 	storw	r4,0xfedcc:l\(r1,r0\)
  78:	cc ed 
  7a:	19 00 52 df 	storw	r5,0xfedcc:l\(r3,r2\)
  7e:	cc ed 
  80:	19 00 60 d5 	storw	r6,0x5edcc:l\(r1,r0\)
  84:	cc ed 
  86:	00 d0       	storw	r0,0x0:s\(r1,r0\)
  88:	00 d0       	storw	r0,0x0:s\(r1,r0\)
  8a:	00 df 0f 00 	storw	r0,0xf:m\(r1,r0\)
  8e:	10 df 0f 00 	storw	r1,0xf:m\(r1,r0\)
  92:	20 df 34 12 	storw	r2,0x1234:m\(r1,r0\)
  96:	32 df cd ab 	storw	r3,0xabcd:m\(r3,r2\)
  9a:	43 df ff af 	storw	r4,0xafff:m\(r4,r3\)
  9e:	13 00 55 da 	storw	r5,0xa1234:l\(r6,r5\)
  a2:	34 12 
  a4:	19 00 00 df 	storw	r0,0xffff1:l\(r1,r0\)
  a8:	f1 ff 
  aa:	19 00 10 df 	storw	r1,0xffff1:l\(r1,r0\)
  ae:	f1 ff 
  b0:	19 00 20 df 	storw	r2,0xfedcc:l\(r1,r0\)
  b4:	cc ed 
  b6:	19 00 32 df 	storw	r3,0xf5433:l\(r3,r2\)
  ba:	33 54 
  bc:	19 00 43 df 	storw	r4,0xf5001:l\(r4,r3\)
  c0:	01 50 
  c2:	19 00 55 d5 	storw	r5,0x5edcc:l\(r6,r5\)
  c6:	cc ed 
  c8:	00 de       	storw	r0,\[r12\]0x0:s\(r1,r0\)
  ca:	18 de       	storw	r1,\[r13\]0x0:s\(r1,r0\)
  cc:	f0 c6 04 12 	storw	r15,\[r12\]0x234:m\(r1,r0\)
  d0:	13 00 38 e1 	storw	r3,\[r13\]0x1abcd:l\(r1,r0\)
  d4:	cd ab 
  d6:	13 00 40 ea 	storw	r4,\[r12\]0xa1234:l\(r1,r0\)
  da:	34 12 
  dc:	13 00 58 eb 	storw	r5,\[r13\]0xb1234:l\(r1,r0\)
  e0:	34 12 
  e2:	13 00 68 ef 	storw	r6,\[r13\]0xfffff:l\(r1,r0\)
  e6:	ff ff 
  e8:	40 c1 cd 0b 	storw	\$0x4:s,0xbcd <main\+0xbcd>:m
  ec:	5a c1 cd ab 	storw	\$0x5:s,0xaabcd <main\+0xaabcd>:m
  f0:	13 00 3f 3a 	storw	\$0x3:s,0xfaabcd <main\+0xfaabcd>:l
  f4:	cd ab 
  f6:	50 c4 14 00 	storw	\$0x5:s,\[r13\]0x14:m
  fa:	40 c5 fc ab 	storw	\$0x4:s,\[r13\]0xabfc:m
  fe:	30 c4 34 12 	storw	\$0x3:s,\[r12\]0x1234:m
 102:	30 c5 34 12 	storw	\$0x3:s,\[r12\]0x1234:m
 106:	30 c4 34 00 	storw	\$0x3:s,\[r12\]0x34:m
 10a:	30 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r1,r0\)
 10e:	31 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r3,r2\)
 112:	36 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r4,r3\)
 116:	32 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r5,r4\)
 11a:	37 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r6,r5\)
 11e:	33 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r7,r6\)
 122:	34 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r9,r8\)
 126:	35 c6 3a 4a 	storw	\$0x3:s,\[r12\]0xa7a:m\(r11,r10\)
 12a:	38 c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r1,r0\)
 12e:	39 c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r3,r2\)
 132:	3e c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r4,r3\)
 136:	3a c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r5,r4\)
 13a:	3f c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r6,r5\)
 13e:	3b c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r7,r6\)
 142:	3c c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r9,r8\)
 146:	3d c6 3a 4a 	storw	\$0x3:s,\[r13\]0xa7a:m\(r11,r10\)
 14a:	3e c6 5a 4b 	storw	\$0x5:s,\[r13\]0xb7a:m\(r4,r3\)
 14e:	37 c6 1a 41 	storw	\$0x1:s,\[r12\]0x17a:m\(r6,r5\)
 152:	3f c6 14 01 	storw	\$0x1:s,\[r13\]0x134:m\(r6,r5\)
 156:	13 00 36 2a 	storw	\$0x3:s,\[r12\]0xabcde:l\(r4,r3\)
 15a:	de bc 
 15c:	13 00 5e 20 	storw	\$0x5:s,\[r13\]0xabcd:l\(r4,r3\)
 160:	cd ab 
 162:	13 00 37 20 	storw	\$0x3:s,\[r12\]0xabcd:l\(r6,r5\)
 166:	cd ab 
 168:	13 00 3f 20 	storw	\$0x3:s,\[r13\]0xbcde:l\(r6,r5\)
 16c:	de bc 
 16e:	13 00 52 00 	storw	\$0x5:s,0x0:l\(r2\)
 172:	00 00 
 174:	3c c3 34 00 	storw	\$0x3:s,0x34:m\(r12\)
 178:	3d c3 ab 00 	storw	\$0x3:s,0xab:m\(r13\)
 17c:	13 00 51 00 	storw	\$0x5:s,0xad:l\(r1\)
 180:	ad 00 
 182:	13 00 52 00 	storw	\$0x5:s,0xcd:l\(r2\)
 186:	cd 00 
 188:	13 00 50 00 	storw	\$0x5:s,0xfff:l\(r0\)
 18c:	ff 0f 
 18e:	13 00 34 00 	storw	\$0x3:s,0xbcd:l\(r4\)
 192:	cd 0b 
 194:	3c c3 ff 0f 	storw	\$0x3:s,0xfff:m\(r12\)
 198:	3d c3 ff 0f 	storw	\$0x3:s,0xfff:m\(r13\)
 19c:	3d c3 ff ff 	storw	\$0x3:s,0xffff:m\(r13\)
 1a0:	3c c3 43 23 	storw	\$0x3:s,0x2343:m\(r12\)
 1a4:	13 00 32 01 	storw	\$0x3:s,0x2345:l\(r2\)
 1a8:	45 23 
 1aa:	13 00 38 04 	storw	\$0x3:s,0xabcd:l\(r8\)
 1ae:	cd ab 
 1b0:	13 00 3d 1f 	storw	\$0x3:s,0xfabcd:l\(r13\)
 1b4:	cd ab 
 1b6:	13 00 38 0f 	storw	\$0x3:s,0xabcd:l\(r8\)
 1ba:	cd ab 
 1bc:	13 00 39 0f 	storw	\$0x3:s,0xabcd:l\(r9\)
 1c0:	cd ab 
 1c2:	13 00 39 04 	storw	\$0x3:s,0xabcd:l\(r9\)
 1c6:	cd ab 
 1c8:	31 c2       	storw	\$0x3:s,0x0:s\(r2,r1\)
 1ca:	51 c3 01 00 	storw	\$0x5:s,0x1:m\(r2,r1\)
 1ce:	41 c3 34 12 	storw	\$0x4:s,0x1234:m\(r2,r1\)
 1d2:	31 c3 34 12 	storw	\$0x3:s,0x1234:m\(r2,r1\)
 1d6:	13 00 31 11 	storw	\$0x3:s,0x12345:l\(r2,r1\)
 1da:	45 23 
 1dc:	31 c3 23 01 	storw	\$0x3:s,0x123:m\(r2,r1\)
 1e0:	13 00 31 11 	storw	\$0x3:s,0x12345:l\(r2,r1\)
 1e4:	45 23 
