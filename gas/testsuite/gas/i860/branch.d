#as:
#objdump: -dr
#name: i860 branch

.*: +file format .*

Disassembly of section .text:

00000000 <.text>:
   0:	3d 00 20 b4 	bla	%r0,%r1,0x000000f8	// 0xf8
   4:	00 00 00 a0 	shl	%r0,%r0,%r0
   8:	3d 28 e0 b7 	bla	%r5,%r31,0x00000100	// 0x100
   c:	00 00 00 a0 	shl	%r0,%r0,%r0
  10:	39 b8 00 b6 	bla	%r23,%r16,0x000000f8	// 0xf8
  14:	00 00 00 a0 	shl	%r0,%r0,%r0
  18:	39 20 60 b6 	bla	%r4,%r19,0x00000100	// 0x100
  1c:	00 00 00 a0 	shl	%r0,%r0,%r0
  20:	00 00 00 40 	bri	%r0
  24:	00 00 00 a0 	shl	%r0,%r0,%r0
  28:	00 08 00 40 	bri	%r1
  2c:	00 00 00 a0 	shl	%r0,%r0,%r0
  30:	00 f8 00 40 	bri	%r31
  34:	00 00 00 a0 	shl	%r0,%r0,%r0
  38:	00 08 00 40 	bri	%r1
  3c:	00 00 00 a0 	shl	%r0,%r0,%r0
  40:	00 60 00 40 	bri	%r12
  44:	00 00 00 a0 	shl	%r0,%r0,%r0
  48:	00 98 00 40 	bri	%r19
  4c:	00 00 00 a0 	shl	%r0,%r0,%r0
  50:	02 00 00 4c 	calli	%r0
  54:	00 00 00 a0 	shl	%r0,%r0,%r0
  58:	02 08 00 4c 	calli	%r1
  5c:	00 00 00 a0 	shl	%r0,%r0,%r0
  60:	02 f8 00 4c 	calli	%r31
  64:	00 00 00 a0 	shl	%r0,%r0,%r0
  68:	02 28 00 4c 	calli	%r5
  6c:	00 00 00 a0 	shl	%r0,%r0,%r0
  70:	02 b0 00 4c 	calli	%r22
  74:	00 00 00 a0 	shl	%r0,%r0,%r0
  78:	02 48 00 4c 	calli	%r9
  7c:	00 00 00 a0 	shl	%r0,%r0,%r0
  80:	1d 00 00 68 	br	0x000000f8	// 0xf8
  84:	00 00 00 a0 	shl	%r0,%r0,%r0
  88:	1d 00 00 68 	br	0x00000100	// 0x100
  8c:	00 00 00 a0 	shl	%r0,%r0,%r0
  90:	00 00 00 68 	br	0x00000094	// 0x94
			90: R_860_PC26	some_fake_extern
  94:	00 00 00 a0 	shl	%r0,%r0,%r0
  98:	17 00 00 6c 	call	0x000000f8	// 0xf8
  9c:	00 00 00 a0 	shl	%r0,%r0,%r0
  a0:	17 00 00 6c 	call	0x00000100	// 0x100
  a4:	00 00 00 a0 	shl	%r0,%r0,%r0
  a8:	00 00 00 6c 	call	0x000000ac	// 0xac
			a8: R_860_PC26	some_fake_extern
  ac:	00 00 00 a0 	shl	%r0,%r0,%r0
  b0:	02 00 00 70 	bc	0x000000bc	// 0xbc
  b4:	10 00 00 70 	bc	0x000000f8	// 0xf8
  b8:	00 00 00 70 	bc	0x000000bc	// 0xbc
			b8: R_860_PC26	some_fake_extern
  bc:	ff ff ff 77 	bc.t	0x000000bc	// 0xbc
  c0:	00 00 00 a0 	shl	%r0,%r0,%r0
  c4:	0c 00 00 74 	bc.t	0x000000f8	// 0xf8
  c8:	00 00 00 a0 	shl	%r0,%r0,%r0
  cc:	00 00 00 74 	bc.t	0x000000d0	// 0xd0
			cc: R_860_PC26	some_fake_extern
  d0:	00 00 00 a0 	shl	%r0,%r0,%r0
  d4:	02 00 00 78 	bnc	0x000000e0	// 0xe0
  d8:	07 00 00 78 	bnc	0x000000f8	// 0xf8
  dc:	00 00 00 78 	bnc	0x000000e0	// 0xe0
			dc: R_860_PC26	some_fake_extern
  e0:	ff ff ff 7f 	bnc.t	0x000000e0	// 0xe0
  e4:	00 00 00 a0 	shl	%r0,%r0,%r0
  e8:	03 00 00 7c 	bnc.t	0x000000f8	// 0xf8
  ec:	00 00 00 a0 	shl	%r0,%r0,%r0
  f0:	00 00 00 7c 	bnc.t	0x000000f4	// 0xf4
			f0: R_860_PC26	some_fake_extern
  f4:	00 00 00 a0 	shl	%r0,%r0,%r0
  f8:	00 00 00 a0 	shl	%r0,%r0,%r0
  fc:	00 00 00 a0 	shl	%r0,%r0,%r0
 100:	00 00 00 a0 	shl	%r0,%r0,%r0
 104:	00 00 00 a0 	shl	%r0,%r0,%r0
