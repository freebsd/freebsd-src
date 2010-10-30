#as:
#objdump:  -dr
#name:  tbitb_test

.*: +file format .*

Disassembly of section .text:

00000000 <main>:
   0:	c0 7b cd 0b 	tbitb	\$0x4,0xbcd <main\+0xbcd>:m
   4:	da 7b cd ab 	tbitb	\$0x5,0xaabcd <main\+0xaabcd>:m
   8:	10 00 3f fa 	tbitb	\$0x3,0xfaabcd <main\+0xfaabcd>:l
   c:	cd ab 
   e:	50 78 14 00 	tbitb	\$0x5,\[r12\]0x14:m
  12:	c0 78 fc ab 	tbitb	\$0x4,\[r13\]0xabfc:m
  16:	30 78 34 12 	tbitb	\$0x3,\[r12\]0x1234:m
  1a:	b0 78 34 12 	tbitb	\$0x3,\[r13\]0x1234:m
  1e:	30 78 34 00 	tbitb	\$0x3,\[r12\]0x34:m
  22:	b0 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r1,r0\)
  26:	b1 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r3,r2\)
  2a:	b6 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r4,r3\)
  2e:	b2 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r5,r4\)
  32:	b7 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r6,r5\)
  36:	b3 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r7,r6\)
  3a:	b4 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r9,r8\)
  3e:	b5 7a 3a 4a 	tbitb	\$0x3,\[r12\]0xa7a:m\(r11,r10\)
  42:	b8 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r1,r0\)
  46:	b9 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r3,r2\)
  4a:	be 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r4,r3\)
  4e:	ba 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r5,r4\)
  52:	bf 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r6,r5\)
  56:	bb 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r7,r6\)
  5a:	bc 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r9,r8\)
  5e:	bd 7a 3a 4a 	tbitb	\$0x3,\[r13\]0xa7a:m\(r11,r10\)
  62:	be 7a 5a 4b 	tbitb	\$0x5,\[r13\]0xb7a:m\(r4,r3\)
  66:	b7 7a 1a 41 	tbitb	\$0x1,\[r12\]0x17a:m\(r6,r5\)
  6a:	bf 7a 14 01 	tbitb	\$0x1,\[r13\]0x134:m\(r6,r5\)
  6e:	10 00 36 ea 	tbitb	\$0x3,\[r12\]0xabcde:l\(r4,r3\)
  72:	de bc 
  74:	10 00 5e e0 	tbitb	\$0x5,\[r13\]0xabcd:l\(r4,r3\)
  78:	cd ab 
  7a:	10 00 37 e0 	tbitb	\$0x3,\[r12\]0xabcd:l\(r6,r5\)
  7e:	cd ab 
  80:	10 00 3f e0 	tbitb	\$0x3,\[r13\]0xbcde:l\(r6,r5\)
  84:	de bc 
  86:	10 00 52 c0 	tbitb	\$0x5,0x0:l\(r2\)
  8a:	00 00 
  8c:	3c 7b 34 00 	tbitb	\$0x3,0x34:m\(r12\)
  90:	3d 7b ab 00 	tbitb	\$0x3,0xab:m\(r13\)
  94:	10 00 51 c0 	tbitb	\$0x5,0xad:l\(r1\)
  98:	ad 00 
  9a:	10 00 52 c0 	tbitb	\$0x5,0xcd:l\(r2\)
  9e:	cd 00 
  a0:	10 00 50 c0 	tbitb	\$0x5,0xfff:l\(r0\)
  a4:	ff 0f 
  a6:	10 00 34 c0 	tbitb	\$0x3,0xbcd:l\(r4\)
  aa:	cd 0b 
  ac:	3c 7b ff 0f 	tbitb	\$0x3,0xfff:m\(r12\)
  b0:	3d 7b ff 0f 	tbitb	\$0x3,0xfff:m\(r13\)
  b4:	3d 7b ff ff 	tbitb	\$0x3,0xffff:m\(r13\)
  b8:	3c 7b 43 23 	tbitb	\$0x3,0x2343:m\(r12\)
  bc:	10 00 32 c1 	tbitb	\$0x3,0x2345:l\(r2\)
  c0:	45 23 
  c2:	10 00 38 c4 	tbitb	\$0x3,0xabcd:l\(r8\)
  c6:	cd ab 
  c8:	10 00 3d df 	tbitb	\$0x3,0xfabcd:l\(r13\)
  cc:	cd ab 
  ce:	10 00 38 cf 	tbitb	\$0x3,0xabcd:l\(r8\)
  d2:	cd ab 
  d4:	10 00 39 cf 	tbitb	\$0x3,0xabcd:l\(r9\)
  d8:	cd ab 
  da:	10 00 39 c4 	tbitb	\$0x3,0xabcd:l\(r9\)
  de:	cd ab 
  e0:	31 7a       	tbitb	\$0x3,0x0:s\(r2,r1\)
  e2:	51 7b 01 00 	tbitb	\$0x5,0x1:m\(r2,r1\)
  e6:	41 7b 34 12 	tbitb	\$0x4,0x1234:m\(r2,r1\)
  ea:	31 7b 34 12 	tbitb	\$0x3,0x1234:m\(r2,r1\)
  ee:	10 00 31 d1 	tbitb	\$0x3,0x12345:l\(r2,r1\)
  f2:	45 23 
  f4:	31 7b 23 01 	tbitb	\$0x3,0x123:m\(r2,r1\)
  f8:	10 00 31 d1 	tbitb	\$0x3,0x12345:l\(r2,r1\)
  fc:	45 23 
