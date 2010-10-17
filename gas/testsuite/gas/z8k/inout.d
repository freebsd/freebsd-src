#as:
#objdump: -dr
#name: inout

.*: +file format coff-z8k

Disassembly of section \.text:

00000000 <\.text>:
   0:	3b14 4444      	in	r1,#0x4444
   4:	3a34 0123      	inb	rh3,#0x123
   8:	3d08           	in	r8,@r0
   a:	3d19           	in	r9,@r1
   c:	3d2a           	in	r10,@r2
   e:	3d3b           	in	r11,@r3
  10:	3d4c           	in	r12,@r4
  12:	3d5d           	in	r13,@r5
  14:	3d6e           	in	r14,@r6
  16:	3d7f           	in	r15,@r7
  18:	3d80           	in	r0,@r8
  1a:	3d91           	in	r1,@r9
  1c:	3da2           	in	r2,@r10
  1e:	3db3           	in	r3,@r11
  20:	3dc4           	in	r4,@r12
  22:	3dd5           	in	r5,@r13
  24:	3de6           	in	r6,@r14
  26:	3df7           	in	r7,@r15
  28:	3c00           	inb	rh0,@r0
  2a:	3c11           	inb	rh1,@r1
  2c:	3c22           	inb	rh2,@r2
  2e:	3c33           	inb	rh3,@r3
  30:	3c44           	inb	rh4,@r4
  32:	3c55           	inb	rh5,@r5
  34:	3c66           	inb	rh6,@r6
  36:	3c77           	inb	rh7,@r7
  38:	3c88           	inb	rl0,@r8
  3a:	3c99           	inb	rl1,@r9
  3c:	3caa           	inb	rl2,@r10
  3e:	3cbb           	inb	rl3,@r11
  40:	3ccc           	inb	rl4,@r12
  42:	3cdd           	inb	rl5,@r13
  44:	3cee           	inb	rl6,@r14
  46:	3cff           	inb	rl7,@r15
  48:	3bf8 0838      	ind	@r3,@r15,r8
  4c:	3be8 0718      	ind	@r1,@r14,r7
  50:	3bd8 0628      	ind	@r2,@r13,r6
  54:	3bc8 0538      	ind	@r3,@r12,r5
  58:	3bb8 0048      	ind	@r4,@r11,r0
  5c:	3ba8 0458      	ind	@r5,@r10,r4
  60:	3b98 0368      	ind	@r6,@r9,r3
  64:	3b88 0278      	ind	@r7,@r8,r2
  68:	3b78 0188      	ind	@r8,@r7,r1
  6c:	3b68 0f98      	ind	@r9,@r6,r15
  70:	3b58 0ea8      	ind	@r10,@r5,r14
  74:	3b48 0db8      	ind	@r11,@r4,r13
  78:	3b38 0bc8      	ind	@r12,@r3,r11
  7c:	3b28 0cd8      	ind	@r13,@r2,r12
  80:	3b18 0ae8      	ind	@r14,@r1,r10
  84:	3b08 09f8      	ind	@r15,@r0,r9
  88:	3af8 0838      	indb	@r3,@r15,r8
  8c:	3ae8 0718      	indb	@r1,@r14,r7
  90:	3ad8 0628      	indb	@r2,@r13,r6
  94:	3ac8 0538      	indb	@r3,@r12,r5
  98:	3ab8 0048      	indb	@r4,@r11,r0
  9c:	3aa8 0458      	indb	@r5,@r10,r4
  a0:	3a98 0368      	indb	@r6,@r9,r3
  a4:	3a88 0278      	indb	@r7,@r8,r2
  a8:	3a78 0188      	indb	@r8,@r7,r1
  ac:	3a68 0f98      	indb	@r9,@r6,r15
  b0:	3a58 0ea8      	indb	@r10,@r5,r14
  b4:	3a48 0db8      	indb	@r11,@r4,r13
  b8:	3a38 0bc8      	indb	@r12,@r3,r11
  bc:	3a28 0cd8      	indb	@r13,@r2,r12
  c0:	3a18 0ae8      	indb	@r14,@r1,r10
  c4:	3a08 09f8      	indb	@r15,@r0,r9
  c8:	3bf8 0830      	indr	@r3,@r15,r8
  cc:	3be8 0710      	indr	@r1,@r14,r7
  d0:	3bd8 0620      	indr	@r2,@r13,r6
  d4:	3bc8 0530      	indr	@r3,@r12,r5
  d8:	3bb8 0040      	indr	@r4,@r11,r0
  dc:	3ba8 0450      	indr	@r5,@r10,r4
  e0:	3b98 0360      	indr	@r6,@r9,r3
  e4:	3b88 0270      	indr	@r7,@r8,r2
  e8:	3b78 0180      	indr	@r8,@r7,r1
  ec:	3b68 0f90      	indr	@r9,@r6,r15
  f0:	3b58 0ea0      	indr	@r10,@r5,r14
  f4:	3b48 0db0      	indr	@r11,@r4,r13
  f8:	3b38 0bc0      	indr	@r12,@r3,r11
  fc:	3b28 0cd0      	indr	@r13,@r2,r12
 100:	3b18 0ae0      	indr	@r14,@r1,r10
 104:	3b08 09f0      	indr	@r15,@r0,r9
 108:	3af8 0830      	indrb	@r3,@r15,r8
 10c:	3ae8 0710      	indrb	@r1,@r14,r7
 110:	3ad8 0620      	indrb	@r2,@r13,r6
 114:	3ac8 0530      	indrb	@r3,@r12,r5
 118:	3ab8 0040      	indrb	@r4,@r11,r0
 11c:	3aa8 0450      	indrb	@r5,@r10,r4
 120:	3a98 0360      	indrb	@r6,@r9,r3
 124:	3a88 0270      	indrb	@r7,@r8,r2
 128:	3a78 0180      	indrb	@r8,@r7,r1
 12c:	3a68 0f90      	indrb	@r9,@r6,r15
 130:	3a58 0ea0      	indrb	@r10,@r5,r14
 134:	3a48 0db0      	indrb	@r11,@r4,r13
 138:	3a38 0bc0      	indrb	@r12,@r3,r11
 13c:	3a28 0cd0      	indrb	@r13,@r2,r12
 140:	3a18 0ae0      	indrb	@r14,@r1,r10
 144:	3a08 09f0      	indrb	@r15,@r0,r9
 148:	3bf0 0838      	ini	@r3,@r15,r8
 14c:	3be0 0718      	ini	@r1,@r14,r7
 150:	3bd0 0628      	ini	@r2,@r13,r6
 154:	3bc0 0538      	ini	@r3,@r12,r5
 158:	3bb0 0048      	ini	@r4,@r11,r0
 15c:	3ba0 0458      	ini	@r5,@r10,r4
 160:	3b90 0368      	ini	@r6,@r9,r3
 164:	3b80 0278      	ini	@r7,@r8,r2
 168:	3b70 0188      	ini	@r8,@r7,r1
 16c:	3b60 0f98      	ini	@r9,@r6,r15
 170:	3b50 0ea8      	ini	@r10,@r5,r14
 174:	3b40 0db8      	ini	@r11,@r4,r13
 178:	3b30 0bc8      	ini	@r12,@r3,r11
 17c:	3b20 0cd8      	ini	@r13,@r2,r12
 180:	3b10 0ae8      	ini	@r14,@r1,r10
 184:	3b00 09f8      	ini	@r15,@r0,r9
 188:	3af0 0838      	inib	@r3,@r15,r8
 18c:	3ae0 0718      	inib	@r1,@r14,r7
 190:	3ad0 0628      	inib	@r2,@r13,r6
 194:	3ac0 0538      	inib	@r3,@r12,r5
 198:	3ab0 0048      	inib	@r4,@r11,r0
 19c:	3aa0 0458      	inib	@r5,@r10,r4
 1a0:	3a90 0368      	inib	@r6,@r9,r3
 1a4:	3a80 0278      	inib	@r7,@r8,r2
 1a8:	3a70 0188      	inib	@r8,@r7,r1
 1ac:	3a60 0f98      	inib	@r9,@r6,r15
 1b0:	3a50 0ea8      	inib	@r10,@r5,r14
 1b4:	3a40 0db8      	inib	@r11,@r4,r13
 1b8:	3a30 0bc8      	inib	@r12,@r3,r11
 1bc:	3a20 0cd8      	inib	@r13,@r2,r12
 1c0:	3a10 0ae8      	inib	@r14,@r1,r10
 1c4:	3a00 09f8      	inib	@r15,@r0,r9
 1c8:	3bf0 0830      	inir	@r3,@r15,r8
 1cc:	3be0 0710      	inir	@r1,@r14,r7
 1d0:	3bd0 0620      	inir	@r2,@r13,r6
 1d4:	3bc0 0530      	inir	@r3,@r12,r5
 1d8:	3bb0 0040      	inir	@r4,@r11,r0
 1dc:	3ba0 0450      	inir	@r5,@r10,r4
 1e0:	3b90 0360      	inir	@r6,@r9,r3
 1e4:	3b80 0270      	inir	@r7,@r8,r2
 1e8:	3b70 0180      	inir	@r8,@r7,r1
 1ec:	3b60 0f90      	inir	@r9,@r6,r15
 1f0:	3b50 0ea0      	inir	@r10,@r5,r14
 1f4:	3b40 0db0      	inir	@r11,@r4,r13
 1f8:	3b30 0bc0      	inir	@r12,@r3,r11
 1fc:	3b20 0cd0      	inir	@r13,@r2,r12
 200:	3b10 0ae0      	inir	@r14,@r1,r10
 204:	3b00 09f0      	inir	@r15,@r0,r9
 208:	3af0 0830      	inirb	@r3,@r15,r8
 20c:	3ae0 0710      	inirb	@r1,@r14,r7
 210:	3ad0 0620      	inirb	@r2,@r13,r6
 214:	3ac0 0530      	inirb	@r3,@r12,r5
 218:	3ab0 0040      	inirb	@r4,@r11,r0
 21c:	3aa0 0450      	inirb	@r5,@r10,r4
 220:	3a90 0360      	inirb	@r6,@r9,r3
 224:	3a80 0270      	inirb	@r7,@r8,r2
 228:	3a70 0180      	inirb	@r8,@r7,r1
 22c:	3a60 0f90      	inirb	@r9,@r6,r15
 230:	3a50 0ea0      	inirb	@r10,@r5,r14
 234:	3a40 0db0      	inirb	@r11,@r4,r13
 238:	3a30 0bc0      	inirb	@r12,@r3,r11
 23c:	3a20 0cd0      	inirb	@r13,@r2,r12
 240:	3a10 0ae0      	inirb	@r14,@r1,r10
 244:	3a00 09f0      	inirb	@r15,@r0,r9
 248:	3b36 1234      	out	#0x1234,r3
 24c:	3aa6 0123      	outb	#0x123,rl2
 250:	3f08           	out	@r0,r8
 252:	3f19           	out	@r1,r9
 254:	3f2a           	out	@r2,r10
 256:	3f3b           	out	@r3,r11
 258:	3f4c           	out	@r4,r12
 25a:	3f5d           	out	@r5,r13
 25c:	3f6e           	out	@r6,r14
 25e:	3f7f           	out	@r7,r15
 260:	3f80           	out	@r8,r0
 262:	3f91           	out	@r9,r1
 264:	3fa2           	out	@r10,r2
 266:	3fb3           	out	@r11,r3
 268:	3fc4           	out	@r12,r4
 26a:	3fd5           	out	@r13,r5
 26c:	3fe6           	out	@r14,r6
 26e:	3ff7           	out	@r15,r7
 270:	3e00           	outb	@r0,rh0
 272:	3e11           	outb	@r1,rh1
 274:	3e22           	outb	@r2,rh2
 276:	3e33           	outb	@r3,rh3
 278:	3e44           	outb	@r4,rh4
 27a:	3e55           	outb	@r5,rh5
 27c:	3e66           	outb	@r6,rh6
 27e:	3e77           	outb	@r7,rh7
 280:	3e88           	outb	@r8,rl0
 282:	3e99           	outb	@r9,rl1
 284:	3eaa           	outb	@r10,rl2
 286:	3ebb           	outb	@r11,rl3
 288:	3ecc           	outb	@r12,rl4
 28a:	3edd           	outb	@r13,rl5
 28c:	3eee           	outb	@r14,rl6
 28e:	3eff           	outb	@r15,rl7
 290:	3bfa 0808      	outd	@r0,@r15,r8
 294:	3bea 0718      	outd	@r1,@r14,r7
 298:	3bda 0628      	outd	@r2,@r13,r6
 29c:	3bca 0538      	outd	@r3,@r12,r5
 2a0:	3bba 0048      	outd	@r4,@r11,r0
 2a4:	3baa 0458      	outd	@r5,@r10,r4
 2a8:	3b9a 0368      	outd	@r6,@r9,r3
 2ac:	3b8a 0278      	outd	@r7,@r8,r2
 2b0:	3b7a 0188      	outd	@r8,@r7,r1
 2b4:	3b6a 0f98      	outd	@r9,@r6,r15
 2b8:	3b5a 0ea8      	outd	@r10,@r5,r14
 2bc:	3b4a 0db8      	outd	@r11,@r4,r13
 2c0:	3b3a 0bc8      	outd	@r12,@r3,r11
 2c4:	3b2a 0cd8      	outd	@r13,@r2,r12
 2c8:	3b1a 0ae8      	outd	@r14,@r1,r10
 2cc:	3b3a 09f8      	outd	@r15,@r3,r9
 2d0:	3afa 0808      	outdb	@r0,@r15,r8
 2d4:	3aea 0718      	outdb	@r1,@r14,r7
 2d8:	3ada 0628      	outdb	@r2,@r13,r6
 2dc:	3aca 0538      	outdb	@r3,@r12,r5
 2e0:	3aba 0048      	outdb	@r4,@r11,r0
 2e4:	3aaa 0458      	outdb	@r5,@r10,r4
 2e8:	3a9a 0368      	outdb	@r6,@r9,r3
 2ec:	3a8a 0278      	outdb	@r7,@r8,r2
 2f0:	3a7a 0188      	outdb	@r8,@r7,r1
 2f4:	3a6a 0f98      	outdb	@r9,@r6,r15
 2f8:	3a5a 0ea8      	outdb	@r10,@r5,r14
 2fc:	3a4a 0db8      	outdb	@r11,@r4,r13
 300:	3a3a 0bc8      	outdb	@r12,@r3,r11
 304:	3a2a 0cd8      	outdb	@r13,@r2,r12
 308:	3a1a 0ae8      	outdb	@r14,@r1,r10
 30c:	3a3a 09f8      	outdb	@r15,@r3,r9
 310:	3bfa 0800      	otdr	@r0,@r15,r8
 314:	3bea 0710      	otdr	@r1,@r14,r7
 318:	3bda 0620      	otdr	@r2,@r13,r6
 31c:	3bca 0530      	otdr	@r3,@r12,r5
 320:	3bba 0040      	otdr	@r4,@r11,r0
 324:	3baa 0450      	otdr	@r5,@r10,r4
 328:	3b9a 0360      	otdr	@r6,@r9,r3
 32c:	3b8a 0270      	otdr	@r7,@r8,r2
 330:	3b7a 0180      	otdr	@r8,@r7,r1
 334:	3b6a 0f90      	otdr	@r9,@r6,r15
 338:	3b5a 0ea0      	otdr	@r10,@r5,r14
 33c:	3b4a 0db0      	otdr	@r11,@r4,r13
 340:	3b3a 0bc0      	otdr	@r12,@r3,r11
 344:	3b2a 0cd0      	otdr	@r13,@r2,r12
 348:	3b1a 0ae0      	otdr	@r14,@r1,r10
 34c:	3b3a 09f0      	otdr	@r15,@r3,r9
 350:	3afa 0800      	otdrb	@r0,@r15,r8
 354:	3aea 0710      	otdrb	@r1,@r14,r7
 358:	3ada 0620      	otdrb	@r2,@r13,r6
 35c:	3aca 0530      	otdrb	@r3,@r12,r5
 360:	3aba 0040      	otdrb	@r4,@r11,r0
 364:	3aaa 0450      	otdrb	@r5,@r10,r4
 368:	3a9a 0360      	otdrb	@r6,@r9,r3
 36c:	3a8a 0270      	otdrb	@r7,@r8,r2
 370:	3a7a 0180      	otdrb	@r8,@r7,r1
 374:	3a6a 0f90      	otdrb	@r9,@r6,r15
 378:	3a5a 0ea0      	otdrb	@r10,@r5,r14
 37c:	3a4a 0db0      	otdrb	@r11,@r4,r13
 380:	3a3a 0bc0      	otdrb	@r12,@r3,r11
 384:	3a2a 0cd0      	otdrb	@r13,@r2,r12
 388:	3a1a 0ae0      	otdrb	@r14,@r1,r10
 38c:	3a3a 09f0      	otdrb	@r15,@r3,r9
 390:	3bf2 0808      	outi	@r0,@r15,r8
 394:	3be2 0718      	outi	@r1,@r14,r7
 398:	3bd2 0628      	outi	@r2,@r13,r6
 39c:	3bc2 0538      	outi	@r3,@r12,r5
 3a0:	3bb2 0048      	outi	@r4,@r11,r0
 3a4:	3ba2 0458      	outi	@r5,@r10,r4
 3a8:	3b92 0368      	outi	@r6,@r9,r3
 3ac:	3b82 0278      	outi	@r7,@r8,r2
 3b0:	3b72 0188      	outi	@r8,@r7,r1
 3b4:	3b62 0f98      	outi	@r9,@r6,r15
 3b8:	3b52 0ea8      	outi	@r10,@r5,r14
 3bc:	3b42 0db8      	outi	@r11,@r4,r13
 3c0:	3b32 0bc8      	outi	@r12,@r3,r11
 3c4:	3b22 0cd8      	outi	@r13,@r2,r12
 3c8:	3b12 0ae8      	outi	@r14,@r1,r10
 3cc:	3b32 09f8      	outi	@r15,@r3,r9
 3d0:	3af2 0808      	outib	@r0,@r15,r8
 3d4:	3ae2 0718      	outib	@r1,@r14,r7
 3d8:	3ad2 0628      	outib	@r2,@r13,r6
 3dc:	3ac2 0538      	outib	@r3,@r12,r5
 3e0:	3ab2 0048      	outib	@r4,@r11,r0
 3e4:	3aa2 0458      	outib	@r5,@r10,r4
 3e8:	3a92 0368      	outib	@r6,@r9,r3
 3ec:	3a82 0278      	outib	@r7,@r8,r2
 3f0:	3a72 0188      	outib	@r8,@r7,r1
 3f4:	3a62 0f98      	outib	@r9,@r6,r15
 3f8:	3a52 0ea8      	outib	@r10,@r5,r14
 3fc:	3a42 0db8      	outib	@r11,@r4,r13
 400:	3a32 0bc8      	outib	@r12,@r3,r11
 404:	3a22 0cd8      	outib	@r13,@r2,r12
 408:	3a12 0ae8      	outib	@r14,@r1,r10
 40c:	3a32 09f8      	outib	@r15,@r3,r9
 410:	3bf2 0800      	otir	@r0,@r15,r8
 414:	3be2 0710      	otir	@r1,@r14,r7
 418:	3bd2 0620      	otir	@r2,@r13,r6
 41c:	3bc2 0530      	otir	@r3,@r12,r5
 420:	3bb2 0040      	otir	@r4,@r11,r0
 424:	3ba2 0450      	otir	@r5,@r10,r4
 428:	3b92 0360      	otir	@r6,@r9,r3
 42c:	3b82 0270      	otir	@r7,@r8,r2
 430:	3b72 0180      	otir	@r8,@r7,r1
 434:	3b62 0f90      	otir	@r9,@r6,r15
 438:	3b52 0ea0      	otir	@r10,@r5,r14
 43c:	3b42 0db0      	otir	@r11,@r4,r13
 440:	3b32 0bc0      	otir	@r12,@r3,r11
 444:	3b22 0cd0      	otir	@r13,@r2,r12
 448:	3b12 0ae0      	otir	@r14,@r1,r10
 44c:	3b32 09f0      	otir	@r15,@r3,r9
 450:	3af2 0800      	otirb	@r0,@r15,r8
 454:	3ae2 0710      	otirb	@r1,@r14,r7
 458:	3ad2 0620      	otirb	@r2,@r13,r6
 45c:	3ac2 0530      	otirb	@r3,@r12,r5
 460:	3ab2 0040      	otirb	@r4,@r11,r0
 464:	3aa2 0450      	otirb	@r5,@r10,r4
 468:	3a92 0360      	otirb	@r6,@r9,r3
 46c:	3a82 0270      	otirb	@r7,@r8,r2
 470:	3a72 0180      	otirb	@r8,@r7,r1
 474:	3a62 0f90      	otirb	@r9,@r6,r15
 478:	3a52 0ea0      	otirb	@r10,@r5,r14
 47c:	3a42 0db0      	otirb	@r11,@r4,r13
 480:	3a32 0bc0      	otirb	@r12,@r3,r11
 484:	3a22 0cd0      	otirb	@r13,@r2,r12
 488:	3a12 0ae0      	otirb	@r14,@r1,r10
 48c:	3a32 09f0      	otirb	@r15,@r3,r9
 490:	3b05 007c      	sin	r0,#0x7c
 494:	3a05 04f2      	sinb	rh0,#0x4f2
 498:	3bf9 0838      	sind	@r3,@r15,r8
 49c:	3be9 0718      	sind	@r1,@r14,r7
 4a0:	3bd9 0628      	sind	@r2,@r13,r6
 4a4:	3bc9 0538      	sind	@r3,@r12,r5
 4a8:	3bb9 0048      	sind	@r4,@r11,r0
 4ac:	3ba9 0458      	sind	@r5,@r10,r4
 4b0:	3b99 0368      	sind	@r6,@r9,r3
 4b4:	3b89 0278      	sind	@r7,@r8,r2
 4b8:	3b79 0188      	sind	@r8,@r7,r1
 4bc:	3b69 0f98      	sind	@r9,@r6,r15
 4c0:	3b59 0ea8      	sind	@r10,@r5,r14
 4c4:	3b49 0db8      	sind	@r11,@r4,r13
 4c8:	3b39 0bc8      	sind	@r12,@r3,r11
 4cc:	3b29 0cd8      	sind	@r13,@r2,r12
 4d0:	3b19 0ae8      	sind	@r14,@r1,r10
 4d4:	3b09 09f8      	sind	@r15,@r0,r9
 4d8:	3af9 0838      	sindb	@r3,@r15,r8
 4dc:	3ae9 0718      	sindb	@r1,@r14,r7
 4e0:	3ad9 0628      	sindb	@r2,@r13,r6
 4e4:	3ac9 0538      	sindb	@r3,@r12,r5
 4e8:	3ab9 0048      	sindb	@r4,@r11,r0
 4ec:	3aa9 0458      	sindb	@r5,@r10,r4
 4f0:	3a99 0368      	sindb	@r6,@r9,r3
 4f4:	3a89 0278      	sindb	@r7,@r8,r2
 4f8:	3a79 0188      	sindb	@r8,@r7,r1
 4fc:	3a69 0f98      	sindb	@r9,@r6,r15
 500:	3a59 0ea8      	sindb	@r10,@r5,r14
 504:	3a49 0db8      	sindb	@r11,@r4,r13
 508:	3a39 0bc8      	sindb	@r12,@r3,r11
 50c:	3a29 0cd8      	sindb	@r13,@r2,r12
 510:	3a19 0ae8      	sindb	@r14,@r1,r10
 514:	3a09 09f8      	sindb	@r15,@r0,r9
 518:	3bf9 0830      	sindr	@r3,@r15,r8
 51c:	3be9 0710      	sindr	@r1,@r14,r7
 520:	3bd9 0620      	sindr	@r2,@r13,r6
 524:	3bc9 0530      	sindr	@r3,@r12,r5
 528:	3bb9 0040      	sindr	@r4,@r11,r0
 52c:	3ba9 0450      	sindr	@r5,@r10,r4
 530:	3b99 0360      	sindr	@r6,@r9,r3
 534:	3b89 0270      	sindr	@r7,@r8,r2
 538:	3b79 0180      	sindr	@r8,@r7,r1
 53c:	3b69 0f90      	sindr	@r9,@r6,r15
 540:	3b59 0ea0      	sindr	@r10,@r5,r14
 544:	3b49 0db0      	sindr	@r11,@r4,r13
 548:	3b39 0bc0      	sindr	@r12,@r3,r11
 54c:	3b29 0cd0      	sindr	@r13,@r2,r12
 550:	3b19 0ae0      	sindr	@r14,@r1,r10
 554:	3b09 09f0      	sindr	@r15,@r0,r9
 558:	3af9 0830      	sindrb	@r3,@r15,r8
 55c:	3ae9 0710      	sindrb	@r1,@r14,r7
 560:	3ad9 0620      	sindrb	@r2,@r13,r6
 564:	3ac9 0530      	sindrb	@r3,@r12,r5
 568:	3ab9 0040      	sindrb	@r4,@r11,r0
 56c:	3aa9 0450      	sindrb	@r5,@r10,r4
 570:	3a99 0360      	sindrb	@r6,@r9,r3
 574:	3a89 0270      	sindrb	@r7,@r8,r2
 578:	3a79 0180      	sindrb	@r8,@r7,r1
 57c:	3a69 0f90      	sindrb	@r9,@r6,r15
 580:	3a59 0ea0      	sindrb	@r10,@r5,r14
 584:	3a49 0db0      	sindrb	@r11,@r4,r13
 588:	3a39 0bc0      	sindrb	@r12,@r3,r11
 58c:	3a29 0cd0      	sindrb	@r13,@r2,r12
 590:	3a19 0ae0      	sindrb	@r14,@r1,r10
 594:	3a09 09f0      	sindrb	@r15,@r0,r9
 598:	3bf1 0838      	sini	@r3,@r15,r8
 59c:	3be1 0718      	sini	@r1,@r14,r7
 5a0:	3bd1 0628      	sini	@r2,@r13,r6
 5a4:	3bc1 0538      	sini	@r3,@r12,r5
 5a8:	3bb1 0048      	sini	@r4,@r11,r0
 5ac:	3ba1 0458      	sini	@r5,@r10,r4
 5b0:	3b91 0368      	sini	@r6,@r9,r3
 5b4:	3b81 0278      	sini	@r7,@r8,r2
 5b8:	3b71 0188      	sini	@r8,@r7,r1
 5bc:	3b61 0f98      	sini	@r9,@r6,r15
 5c0:	3b51 0ea8      	sini	@r10,@r5,r14
 5c4:	3b41 0db8      	sini	@r11,@r4,r13
 5c8:	3b31 0bc8      	sini	@r12,@r3,r11
 5cc:	3b21 0cd8      	sini	@r13,@r2,r12
 5d0:	3b11 0ae8      	sini	@r14,@r1,r10
 5d4:	3b01 09f8      	sini	@r15,@r0,r9
 5d8:	3af1 0838      	sinib	@r3,@r15,r8
 5dc:	3ae1 0718      	sinib	@r1,@r14,r7
 5e0:	3ad1 0628      	sinib	@r2,@r13,r6
 5e4:	3ac1 0538      	sinib	@r3,@r12,r5
 5e8:	3ab1 0048      	sinib	@r4,@r11,r0
 5ec:	3aa1 0458      	sinib	@r5,@r10,r4
 5f0:	3a91 0368      	sinib	@r6,@r9,r3
 5f4:	3a81 0278      	sinib	@r7,@r8,r2
 5f8:	3a71 0188      	sinib	@r8,@r7,r1
 5fc:	3a61 0f98      	sinib	@r9,@r6,r15
 600:	3a51 0ea8      	sinib	@r10,@r5,r14
 604:	3a41 0db8      	sinib	@r11,@r4,r13
 608:	3a31 0bc8      	sinib	@r12,@r3,r11
 60c:	3a21 0cd8      	sinib	@r13,@r2,r12
 610:	3a11 0ae8      	sinib	@r14,@r1,r10
 614:	3a01 09f8      	sinib	@r15,@r0,r9
 618:	3bf1 0830      	sinir	@r3,@r15,r8
 61c:	3be1 0710      	sinir	@r1,@r14,r7
 620:	3bd1 0620      	sinir	@r2,@r13,r6
 624:	3bc1 0530      	sinir	@r3,@r12,r5
 628:	3bb1 0040      	sinir	@r4,@r11,r0
 62c:	3ba1 0450      	sinir	@r5,@r10,r4
 630:	3b91 0360      	sinir	@r6,@r9,r3
 634:	3b81 0270      	sinir	@r7,@r8,r2
 638:	3b71 0180      	sinir	@r8,@r7,r1
 63c:	3b61 0f90      	sinir	@r9,@r6,r15
 640:	3b51 0ea0      	sinir	@r10,@r5,r14
 644:	3b41 0db0      	sinir	@r11,@r4,r13
 648:	3b31 0bc0      	sinir	@r12,@r3,r11
 64c:	3b21 0cd0      	sinir	@r13,@r2,r12
 650:	3b11 0ae0      	sinir	@r14,@r1,r10
 654:	3b01 09f0      	sinir	@r15,@r0,r9
 658:	3af1 0830      	sinirb	@r3,@r15,r8
 65c:	3ae1 0710      	sinirb	@r1,@r14,r7
 660:	3ad1 0620      	sinirb	@r2,@r13,r6
 664:	3ac1 0530      	sinirb	@r3,@r12,r5
 668:	3ab1 0040      	sinirb	@r4,@r11,r0
 66c:	3aa1 0450      	sinirb	@r5,@r10,r4
 670:	3a91 0360      	sinirb	@r6,@r9,r3
 674:	3a81 0270      	sinirb	@r7,@r8,r2
 678:	3a71 0180      	sinirb	@r8,@r7,r1
 67c:	3a61 0f90      	sinirb	@r9,@r6,r15
 680:	3a51 0ea0      	sinirb	@r10,@r5,r14
 684:	3a41 0db0      	sinirb	@r11,@r4,r13
 688:	3a31 0bc0      	sinirb	@r12,@r3,r11
 68c:	3a21 0cd0      	sinirb	@r13,@r2,r12
 690:	3a11 0ae0      	sinirb	@r14,@r1,r10
 694:	3a01 09f0      	sinirb	@r15,@r0,r9
 698:	3b06 beee      	out	#0xbeee,r0
 69c:	3a46 babe      	outb	#0xbabe,rh4
 6a0:	3bfb 0808      	soutd	@r0,@r15,r8
 6a4:	3beb 0718      	soutd	@r1,@r14,r7
 6a8:	3bdb 0628      	soutd	@r2,@r13,r6
 6ac:	3bcb 0538      	soutd	@r3,@r12,r5
 6b0:	3bbb 0048      	soutd	@r4,@r11,r0
 6b4:	3bab 0458      	soutd	@r5,@r10,r4
 6b8:	3b9b 0368      	soutd	@r6,@r9,r3
 6bc:	3b8b 0278      	soutd	@r7,@r8,r2
 6c0:	3b7b 0188      	soutd	@r8,@r7,r1
 6c4:	3b6b 0f98      	soutd	@r9,@r6,r15
 6c8:	3b5b 0ea8      	soutd	@r10,@r5,r14
 6cc:	3b4b 0db8      	soutd	@r11,@r4,r13
 6d0:	3b3b 0bc8      	soutd	@r12,@r3,r11
 6d4:	3b2b 0cd8      	soutd	@r13,@r2,r12
 6d8:	3b1b 0ae8      	soutd	@r14,@r1,r10
 6dc:	3b3b 09f8      	soutd	@r15,@r3,r9
 6e0:	3afb 0808      	soutdb	@r0,@r15,r8
 6e4:	3aeb 0718      	soutdb	@r1,@r14,r7
 6e8:	3adb 0628      	soutdb	@r2,@r13,r6
 6ec:	3acb 0538      	soutdb	@r3,@r12,r5
 6f0:	3abb 0048      	soutdb	@r4,@r11,r0
 6f4:	3aab 0458      	soutdb	@r5,@r10,r4
 6f8:	3a9b 0368      	soutdb	@r6,@r9,r3
 6fc:	3a8b 0278      	soutdb	@r7,@r8,r2
 700:	3a7b 0188      	soutdb	@r8,@r7,r1
 704:	3a6b 0f98      	soutdb	@r9,@r6,r15
 708:	3a5b 0ea8      	soutdb	@r10,@r5,r14
 70c:	3a4b 0db8      	soutdb	@r11,@r4,r13
 710:	3a3b 0bc8      	soutdb	@r12,@r3,r11
 714:	3a2b 0cd8      	soutdb	@r13,@r2,r12
 718:	3a1b 0ae8      	soutdb	@r14,@r1,r10
 71c:	3a3b 09f8      	soutdb	@r15,@r3,r9
 720:	3bfb 0800      	sotdr	@r0,@r15,r8
 724:	3beb 0710      	sotdr	@r1,@r14,r7
 728:	3bdb 0620      	sotdr	@r2,@r13,r6
 72c:	3bcb 0530      	sotdr	@r3,@r12,r5
 730:	3bbb 0040      	sotdr	@r4,@r11,r0
 734:	3bab 0450      	sotdr	@r5,@r10,r4
 738:	3b9b 0360      	sotdr	@r6,@r9,r3
 73c:	3b8b 0270      	sotdr	@r7,@r8,r2
 740:	3b7b 0180      	sotdr	@r8,@r7,r1
 744:	3b6b 0f90      	sotdr	@r9,@r6,r15
 748:	3b5b 0ea0      	sotdr	@r10,@r5,r14
 74c:	3b4b 0db0      	sotdr	@r11,@r4,r13
 750:	3b3b 0bc0      	sotdr	@r12,@r3,r11
 754:	3b2b 0cd0      	sotdr	@r13,@r2,r12
 758:	3b1b 0ae0      	sotdr	@r14,@r1,r10
 75c:	3b3b 09f0      	sotdr	@r15,@r3,r9
 760:	3afb 0800      	sotdrb	@r0,@r15,r8
 764:	3aeb 0710      	sotdrb	@r1,@r14,r7
 768:	3adb 0620      	sotdrb	@r2,@r13,r6
 76c:	3acb 0530      	sotdrb	@r3,@r12,r5
 770:	3abb 0040      	sotdrb	@r4,@r11,r0
 774:	3aab 0450      	sotdrb	@r5,@r10,r4
 778:	3a9b 0360      	sotdrb	@r6,@r9,r3
 77c:	3a8b 0270      	sotdrb	@r7,@r8,r2
 780:	3a7b 0180      	sotdrb	@r8,@r7,r1
 784:	3a6b 0f90      	sotdrb	@r9,@r6,r15
 788:	3a5b 0ea0      	sotdrb	@r10,@r5,r14
 78c:	3a4b 0db0      	sotdrb	@r11,@r4,r13
 790:	3a3b 0bc0      	sotdrb	@r12,@r3,r11
 794:	3a2b 0cd0      	sotdrb	@r13,@r2,r12
 798:	3a1b 0ae0      	sotdrb	@r14,@r1,r10
 79c:	3a3b 09f0      	sotdrb	@r15,@r3,r9
 7a0:	3bf3 0808      	souti	@r0,@r15,r8
 7a4:	3be3 0718      	souti	@r1,@r14,r7
 7a8:	3bd3 0628      	souti	@r2,@r13,r6
 7ac:	3bc3 0538      	souti	@r3,@r12,r5
 7b0:	3bb3 0048      	souti	@r4,@r11,r0
 7b4:	3ba3 0458      	souti	@r5,@r10,r4
 7b8:	3b93 0368      	souti	@r6,@r9,r3
 7bc:	3b83 0278      	souti	@r7,@r8,r2
 7c0:	3b73 0188      	souti	@r8,@r7,r1
 7c4:	3b63 0f98      	souti	@r9,@r6,r15
 7c8:	3b53 0ea8      	souti	@r10,@r5,r14
 7cc:	3b43 0db8      	souti	@r11,@r4,r13
 7d0:	3b33 0bc8      	souti	@r12,@r3,r11
 7d4:	3b23 0cd8      	souti	@r13,@r2,r12
 7d8:	3b13 0ae8      	souti	@r14,@r1,r10
 7dc:	3b33 09f8      	souti	@r15,@r3,r9
 7e0:	3af3 0808      	soutib	@r0,@r15,r8
 7e4:	3ae3 0718      	soutib	@r1,@r14,r7
 7e8:	3ad3 0628      	soutib	@r2,@r13,r6
 7ec:	3ac3 0538      	soutib	@r3,@r12,r5
 7f0:	3ab3 0048      	soutib	@r4,@r11,r0
 7f4:	3aa3 0458      	soutib	@r5,@r10,r4
 7f8:	3a93 0368      	soutib	@r6,@r9,r3
 7fc:	3a83 0278      	soutib	@r7,@r8,r2
 800:	3a73 0188      	soutib	@r8,@r7,r1
 804:	3a63 0f98      	soutib	@r9,@r6,r15
 808:	3a53 0ea8      	soutib	@r10,@r5,r14
 80c:	3a43 0db8      	soutib	@r11,@r4,r13
 810:	3a33 0bc8      	soutib	@r12,@r3,r11
 814:	3a23 0cd8      	soutib	@r13,@r2,r12
 818:	3a13 0ae8      	soutib	@r14,@r1,r10
 81c:	3a33 09f8      	soutib	@r15,@r3,r9
 820:	3bf3 0800      	sotir	@r0,@r15,r8
 824:	3be3 0710      	sotir	@r1,@r14,r7
 828:	3bd3 0620      	sotir	@r2,@r13,r6
 82c:	3bc3 0530      	sotir	@r3,@r12,r5
 830:	3bb3 0040      	sotir	@r4,@r11,r0
 834:	3ba3 0450      	sotir	@r5,@r10,r4
 838:	3b93 0360      	sotir	@r6,@r9,r3
 83c:	3b83 0270      	sotir	@r7,@r8,r2
 840:	3b73 0180      	sotir	@r8,@r7,r1
 844:	3b63 0f90      	sotir	@r9,@r6,r15
 848:	3b53 0ea0      	sotir	@r10,@r5,r14
 84c:	3b43 0db0      	sotir	@r11,@r4,r13
 850:	3b33 0bc0      	sotir	@r12,@r3,r11
 854:	3b23 0cd0      	sotir	@r13,@r2,r12
 858:	3b13 0ae0      	sotir	@r14,@r1,r10
 85c:	3b33 09f0      	sotir	@r15,@r3,r9
 860:	3af3 0800      	sotirb	@r0,@r15,r8
 864:	3ae3 0710      	sotirb	@r1,@r14,r7
 868:	3ad3 0620      	sotirb	@r2,@r13,r6
 86c:	3ac3 0530      	sotirb	@r3,@r12,r5
 870:	3ab3 0040      	sotirb	@r4,@r11,r0
 874:	3aa3 0450      	sotirb	@r5,@r10,r4
 878:	3a93 0360      	sotirb	@r6,@r9,r3
 87c:	3a83 0270      	sotirb	@r7,@r8,r2
 880:	3a73 0180      	sotirb	@r8,@r7,r1
 884:	3a63 0f90      	sotirb	@r9,@r6,r15
 888:	3a53 0ea0      	sotirb	@r10,@r5,r14
 88c:	3a43 0db0      	sotirb	@r11,@r4,r13
 890:	3a33 0bc0      	sotirb	@r12,@r3,r11
 894:	3a23 0cd0      	sotirb	@r13,@r2,r12
 898:	3a13 0ae0      	sotirb	@r14,@r1,r10
 89c:	3a33 09f0      	sotirb	@r15,@r3,r9
