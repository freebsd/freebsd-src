#as:
#objdump:  -dr
#name:  sbitw_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	40 77 cd 0b 	sbitw	\$0x4:s,0xbcd <main\+0xbcd>:m
   4:	5a 77 cd ab 	sbitw	\$0x5:s,0xaabcd <main\+0xaabcd>:m
   8:	11 00 3f ba 	sbitw	\$0x3:s,0xfaabcd <main\+0xfaabcd>:l
   c:	cd ab 
   e:	a0 77 cd 0b 	sbitw	\$0xa:s,0xbcd <main\+0xbcd>:m
  12:	fa 77 cd ab 	sbitw	\$0xf:s,0xaabcd <main\+0xaabcd>:m
  16:	11 00 ef ba 	sbitw	\$0xe:s,0xfaabcd <main\+0xfaabcd>:l
  1a:	cd ab 
  1c:	50 74 14 00 	sbitw	\$0x5:s,\[r13\]0x14:m
  20:	40 75 fc ab 	sbitw	\$0x4:s,\[r13\]0xabfc:m
  24:	30 74 34 12 	sbitw	\$0x3:s,\[r12\]0x1234:m
  28:	30 75 34 12 	sbitw	\$0x3:s,\[r12\]0x1234:m
  2c:	30 74 34 00 	sbitw	\$0x3:s,\[r12\]0x34:m
  30:	f0 74 14 00 	sbitw	\$0xf:s,\[r13\]0x14:m
  34:	e0 75 fc ab 	sbitw	\$0xe:s,\[r13\]0xabfc:m
  38:	d0 74 34 12 	sbitw	\$0xd:s,\[r13\]0x1234:m
  3c:	d0 75 34 12 	sbitw	\$0xd:s,\[r13\]0x1234:m
  40:	b0 74 34 00 	sbitw	\$0xb:s,\[r12\]0x34:m
  44:	f0 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r1,r0\)
  48:	f1 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r3,r2\)
  4c:	f6 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r4,r3\)
  50:	f2 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r5,r4\)
  54:	f7 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r6,r5\)
  58:	f3 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r7,r6\)
  5c:	f4 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r9,r8\)
  60:	f5 72 3a 4a 	sbitw	\$0x3:s,\[r12\]0xa7a:m\(r11,r10\)
  64:	f8 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r1,r0\)
  68:	f9 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r3,r2\)
  6c:	fe 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r4,r3\)
  70:	fa 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r5,r4\)
  74:	ff 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r6,r5\)
  78:	fb 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r7,r6\)
  7c:	fc 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r9,r8\)
  80:	fd 72 3a 4a 	sbitw	\$0x3:s,\[r13\]0xa7a:m\(r11,r10\)
  84:	fe 72 5a 4b 	sbitw	\$0x5:s,\[r13\]0xb7a:m\(r4,r3\)
  88:	f7 72 1a 41 	sbitw	\$0x1:s,\[r12\]0x17a:m\(r6,r5\)
  8c:	ff 72 14 01 	sbitw	\$0x1:s,\[r13\]0x134:m\(r6,r5\)
  90:	11 00 36 aa 	sbitw	\$0x3:s,\[r12\]0xabcde:l\(r4,r3\)
  94:	de bc 
  96:	11 00 5e a0 	sbitw	\$0x5:s,\[r13\]0xabcd:l\(r4,r3\)
  9a:	cd ab 
  9c:	11 00 37 a0 	sbitw	\$0x3:s,\[r12\]0xabcd:l\(r6,r5\)
  a0:	cd ab 
  a2:	11 00 3f a0 	sbitw	\$0x3:s,\[r13\]0xbcde:l\(r6,r5\)
  a6:	de bc 
  a8:	f0 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r1,r0\)
  ac:	f1 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r3,r2\)
  b0:	f6 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r4,r3\)
  b4:	f2 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r5,r4\)
  b8:	f7 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r6,r5\)
  bc:	f3 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r7,r6\)
  c0:	f4 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r9,r8\)
  c4:	f5 72 da 4a 	sbitw	\$0xd:s,\[r12\]0xafa:m\(r11,r10\)
  c8:	f8 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r1,r0\)
  cc:	f9 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r3,r2\)
  d0:	fe 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r4,r3\)
  d4:	fa 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r5,r4\)
  d8:	ff 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r6,r5\)
  dc:	fb 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r7,r6\)
  e0:	fc 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r9,r8\)
  e4:	fd 72 da 4a 	sbitw	\$0xd:s,\[r13\]0xafa:m\(r11,r10\)
  e8:	fe 72 fa 4b 	sbitw	\$0xf:s,\[r13\]0xbfa:m\(r4,r3\)
  ec:	f7 72 ba 41 	sbitw	\$0xb:s,\[r12\]0x1fa:m\(r6,r5\)
  f0:	ff 72 b4 01 	sbitw	\$0xb:s,\[r13\]0x1b4:m\(r6,r5\)
  f4:	11 00 d6 aa 	sbitw	\$0xd:s,\[r12\]0xabcde:l\(r4,r3\)
  f8:	de bc 
  fa:	11 00 fe a0 	sbitw	\$0xf:s,\[r13\]0xabcd:l\(r4,r3\)
  fe:	cd ab 
 100:	11 00 d7 a0 	sbitw	\$0xd:s,\[r12\]0xabcd:l\(r6,r5\)
 104:	cd ab 
 106:	11 00 df a0 	sbitw	\$0xd:s,\[r13\]0xbcde:l\(r6,r5\)
 10a:	de bc 
 10c:	11 00 52 80 	sbitw	\$0x5:s,0x0:l\(r2\)
 110:	00 00 
 112:	3c 71 34 00 	sbitw	\$0x3:s,0x34:m\(r12\)
 116:	3d 71 ab 00 	sbitw	\$0x3:s,0xab:m\(r13\)
 11a:	11 00 51 80 	sbitw	\$0x5:s,0xad:l\(r1\)
 11e:	ad 00 
 120:	11 00 52 80 	sbitw	\$0x5:s,0xcd:l\(r2\)
 124:	cd 00 
 126:	11 00 50 80 	sbitw	\$0x5:s,0xfff:l\(r0\)
 12a:	ff 0f 
 12c:	11 00 34 80 	sbitw	\$0x3:s,0xbcd:l\(r4\)
 130:	cd 0b 
 132:	3c 71 ff 0f 	sbitw	\$0x3:s,0xfff:m\(r12\)
 136:	3d 71 ff 0f 	sbitw	\$0x3:s,0xfff:m\(r13\)
 13a:	3d 71 ff ff 	sbitw	\$0x3:s,0xffff:m\(r13\)
 13e:	3c 71 43 23 	sbitw	\$0x3:s,0x2343:m\(r12\)
 142:	11 00 32 81 	sbitw	\$0x3:s,0x2345:l\(r2\)
 146:	45 23 
 148:	11 00 38 84 	sbitw	\$0x3:s,0xabcd:l\(r8\)
 14c:	cd ab 
 14e:	11 00 3d 9f 	sbitw	\$0x3:s,0xfabcd:l\(r13\)
 152:	cd ab 
 154:	11 00 38 8f 	sbitw	\$0x3:s,0xabcd:l\(r8\)
 158:	cd ab 
 15a:	11 00 39 8f 	sbitw	\$0x3:s,0xabcd:l\(r9\)
 15e:	cd ab 
 160:	11 00 39 84 	sbitw	\$0x3:s,0xabcd:l\(r9\)
 164:	cd ab 
 166:	11 00 f2 80 	sbitw	\$0xf:s,0x0:l\(r2\)
 16a:	00 00 
 16c:	dc 71 34 00 	sbitw	\$0xd:s,0x34:m\(r12\)
 170:	dd 71 ab 00 	sbitw	\$0xd:s,0xab:m\(r13\)
 174:	11 00 f1 80 	sbitw	\$0xf:s,0xad:l\(r1\)
 178:	ad 00 
 17a:	11 00 f2 80 	sbitw	\$0xf:s,0xcd:l\(r2\)
 17e:	cd 00 
 180:	11 00 f0 80 	sbitw	\$0xf:s,0xfff:l\(r0\)
 184:	ff 0f 
 186:	11 00 d4 80 	sbitw	\$0xd:s,0xbcd:l\(r4\)
 18a:	cd 0b 
 18c:	dc 71 ff 0f 	sbitw	\$0xd:s,0xfff:m\(r12\)
 190:	dd 71 ff 0f 	sbitw	\$0xd:s,0xfff:m\(r13\)
 194:	dd 71 ff ff 	sbitw	\$0xd:s,0xffff:m\(r13\)
 198:	dc 71 43 23 	sbitw	\$0xd:s,0x2343:m\(r12\)
 19c:	11 00 d2 81 	sbitw	\$0xd:s,0x2345:l\(r2\)
 1a0:	45 23 
 1a2:	11 00 d8 84 	sbitw	\$0xd:s,0xabcd:l\(r8\)
 1a6:	cd ab 
 1a8:	11 00 dd 9f 	sbitw	\$0xd:s,0xfabcd:l\(r13\)
 1ac:	cd ab 
 1ae:	11 00 d8 8f 	sbitw	\$0xd:s,0xabcd:l\(r8\)
 1b2:	cd ab 
 1b4:	11 00 d9 8f 	sbitw	\$0xd:s,0xabcd:l\(r9\)
 1b8:	cd ab 
 1ba:	11 00 d9 84 	sbitw	\$0xd:s,0xabcd:l\(r9\)
 1be:	cd ab 
 1c0:	31 76       	sbitw	\$0x3:s,0x0:s\(r2,r1\)
 1c2:	51 71 01 00 	sbitw	\$0x5:s,0x1:m\(r2,r1\)
 1c6:	41 71 34 12 	sbitw	\$0x4:s,0x1234:m\(r2,r1\)
 1ca:	31 71 34 12 	sbitw	\$0x3:s,0x1234:m\(r2,r1\)
 1ce:	11 00 31 91 	sbitw	\$0x3:s,0x12345:l\(r2,r1\)
 1d2:	45 23 
 1d4:	31 71 23 01 	sbitw	\$0x3:s,0x123:m\(r2,r1\)
 1d8:	11 00 31 91 	sbitw	\$0x3:s,0x12345:l\(r2,r1\)
 1dc:	45 23 
 1de:	d1 76       	sbitw	\$0xd:s,0x0:s\(r2,r1\)
 1e0:	f1 71 01 00 	sbitw	\$0xf:s,0x1:m\(r2,r1\)
 1e4:	e1 71 34 12 	sbitw	\$0xe:s,0x1234:m\(r2,r1\)
 1e8:	d1 71 34 12 	sbitw	\$0xd:s,0x1234:m\(r2,r1\)
 1ec:	11 00 d1 91 	sbitw	\$0xd:s,0x12345:l\(r2,r1\)
 1f0:	45 23 
 1f2:	d1 71 23 01 	sbitw	\$0xd:s,0x123:m\(r2,r1\)
 1f6:	11 00 d1 91 	sbitw	\$0xd:s,0x12345:l\(r2,r1\)
 1fa:	45 23 
