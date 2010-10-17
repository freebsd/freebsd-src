#as:
#objdump: -dr
#name: i860 bte/btne

.*: +file format .*

Disassembly of section \.text:

00000000 <some_label-0xb8>:
   0:	2d 00 e0 57 	btne	0,%r31,0x000000b8	// b8 <some_label>
   4:	2c 08 a0 57 	btne	1,%r29,0x000000b8	// b8 <some_label>
   8:	2b 10 60 57 	btne	2,%r27,0x000000b8	// b8 <some_label>
   c:	2a 18 20 57 	btne	3,%r25,0x000000b8	// b8 <some_label>
  10:	29 50 e0 56 	btne	10,%r23,0x000000b8	// b8 <some_label>
  14:	28 58 a0 56 	btne	11,%r21,0x000000b8	// b8 <some_label>
  18:	27 60 60 56 	btne	12,%r19,0x000000b8	// b8 <some_label>
  1c:	26 e8 20 56 	btne	29,%r17,0x000000b8	// b8 <some_label>
  20:	25 f0 00 56 	btne	30,%r16,0x000000b8	// b8 <some_label>
  24:	24 f8 00 55 	btne	31,%r8,0x000000b8	// b8 <some_label>
  28:	00 78 00 54 	btne	15,%r0,0x0000002c	// 2c <some_label-0x8c>
			28: R_860_PC16	some_fake_extern
  2c:	22 00 e0 5f 	bte	0,%r31,0x000000b8	// b8 <some_label>
  30:	21 08 a0 5f 	bte	1,%r29,0x000000b8	// b8 <some_label>
  34:	20 10 60 5f 	bte	2,%r27,0x000000b8	// b8 <some_label>
  38:	1f 18 20 5f 	bte	3,%r25,0x000000b8	// b8 <some_label>
  3c:	1e 50 e0 5e 	bte	10,%r23,0x000000b8	// b8 <some_label>
  40:	1d 58 a0 5e 	bte	11,%r21,0x000000b8	// b8 <some_label>
  44:	1c 60 60 5e 	bte	12,%r19,0x000000b8	// b8 <some_label>
  48:	1b e8 20 5e 	bte	29,%r17,0x000000b8	// b8 <some_label>
  4c:	1a f0 00 5e 	bte	30,%r16,0x000000b8	// b8 <some_label>
  50:	19 f8 00 5d 	bte	31,%r8,0x000000b8	// b8 <some_label>
  54:	00 78 00 5c 	bte	15,%r0,0x00000058	// 58 <some_label-0x60>
			54: R_860_PC16	some_fake_extern
  58:	17 00 e0 53 	btne	%r0,%r31,0x000000b8	// b8 <some_label>
  5c:	16 08 a0 53 	btne	%r1,%r29,0x000000b8	// b8 <some_label>
  60:	15 10 60 53 	btne	%sp,%r27,0x000000b8	// b8 <some_label>
  64:	14 18 20 53 	btne	%fp,%r25,0x000000b8	// b8 <some_label>
  68:	13 50 e0 52 	btne	%r10,%r23,0x000000b8	// b8 <some_label>
  6c:	12 58 a0 52 	btne	%r11,%r21,0x000000b8	// b8 <some_label>
  70:	11 60 60 52 	btne	%r12,%r19,0x000000b8	// b8 <some_label>
  74:	10 e8 20 52 	btne	%r29,%r17,0x000000b8	// b8 <some_label>
  78:	0f f0 00 52 	btne	%r30,%r16,0x000000b8	// b8 <some_label>
  7c:	0e f8 00 51 	btne	%r31,%r8,0x000000b8	// b8 <some_label>
  80:	00 78 00 50 	btne	%r15,%r0,0x00000084	// 84 <some_label-0x34>
			80: R_860_PC16	some_fake_extern
  84:	0c 00 e0 5b 	bte	%r0,%r31,0x000000b8	// b8 <some_label>
  88:	0b 08 a0 5b 	bte	%r1,%r29,0x000000b8	// b8 <some_label>
  8c:	0a 10 60 5b 	bte	%sp,%r27,0x000000b8	// b8 <some_label>
  90:	09 18 20 5b 	bte	%fp,%r25,0x000000b8	// b8 <some_label>
  94:	08 50 e0 5a 	bte	%r10,%r23,0x000000b8	// b8 <some_label>
  98:	07 58 a0 5a 	bte	%r11,%r21,0x000000b8	// b8 <some_label>
  9c:	06 60 60 5a 	bte	%r12,%r19,0x000000b8	// b8 <some_label>
  a0:	05 e8 20 5a 	bte	%r29,%r17,0x000000b8	// b8 <some_label>
  a4:	04 f0 00 5a 	bte	%r30,%r16,0x000000b8	// b8 <some_label>
  a8:	03 f8 00 59 	bte	%r31,%r8,0x000000b8	// b8 <some_label>
  ac:	00 78 00 58 	bte	%r15,%r0,0x000000b0	// b0 <some_label-0x8>
			ac: R_860_PC16	some_fake_extern
  b0:	00 00 00 a0 	shl	%r0,%r0,%r0
  b4:	00 00 00 a0 	shl	%r0,%r0,%r0

000000b8 <some_label>:
  b8:	00 00 00 a0 	shl	%r0,%r0,%r0
