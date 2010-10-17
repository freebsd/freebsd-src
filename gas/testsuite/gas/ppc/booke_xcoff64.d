#as: -a64 -mppc64 -mbooke64
#objdump: -dr -Mbooke64
#name: xcoff64 BookE tests

.*:     file format aix5?coff64-rs6000

Disassembly of section .text:

0000000000000000 <.text>:
   0:	7c 22 3f 64 	tlbre   r1,r2,7
   4:	7c be 1f a4 	tlbwe   r5,r30,3
   8:	24 25 00 30 	bce     1,4\*cr1\+gt,38 <.text\+0x38>
   c:	24 46 00 3d 	bcel    2,4\*cr1\+eq,48 <.text\+0x48>
  10:	24 67 00 5a 	bcea    3,4\*cr1\+so,58 <.text\+0x58>
			12: R_BA_16	.text
  14:	24 88 00 7b 	bcela   4,4\*cr2,78 <.text\+0x78>
			16: R_BA_16	.text
  18:	4c a9 00 22 	bclre   5,4\*cr2\+gt
  1c:	4c aa 00 23 	bclrel  5,4\*cr2\+eq
  20:	4d 0b 04 22 	bcctre  8,4\*cr2\+so
  24:	4d 0c 04 23 	bcctrel 8,4\*cr3
  28:	58 00 00 74 	be      9c <.text\+0x9c>
  2c:	58 00 00 89 	bel     b4 <.text\+0xb4>
  30:	58 00 00 f2 	bea     f0 <.text\+0xf0>
			30: R_BA_26	.text
  34:	58 00 01 27 	bela    124 <.text\+0x124>
			34: R_BA_26	.text
  38:	e9 09 00 80 	lbze    r8,8\(r9\)
  3c:	e9 8f 00 41 	lbzue   r12,4\(r15\)
  40:	7c 86 40 fe 	lbzuxe  r4,r6,r8
  44:	7c 65 38 be 	lbzxe   r3,r5,r7
  48:	f8 a6 06 40 	lde     r5,400\(r6\)
  4c:	f8 c7 07 11 	ldue    r6,452\(r7\)
  50:	7c e8 4e 3e 	ldxe    r7,r8,r9
  54:	7d 4b 66 7e 	lduxe   r10,r11,r12
  58:	f9 81 02 06 	lfde    f12,128\(r1\)
  5c:	f8 25 00 47 	lfdue   f1,16\(r5\)
  60:	7c a1 1c be 	lfdxe   f5,r1,r3
  64:	7c c2 24 fe 	lfduxe  f6,r2,r4
  68:	f9 09 00 c4 	lfse    f8,48\(r9\)
  6c:	f9 2a 01 15 	lfsue   f9,68\(r10\)
  70:	7d 44 44 7e 	lfsuxe  f10,r4,r8
  74:	7d 23 3c 3e 	lfsxe   f9,r3,r7
  78:	e9 45 03 24 	lhae    r10,50\(r5\)
  7c:	e8 23 00 55 	lhaue   r1,5\(r3\)
  80:	7c a1 1a fe 	lhauxe  r5,r1,r3
  84:	7f be fa be 	lhaxe   r29,r30,r31
  88:	7c 22 1e 3c 	lhbrxe  r1,r2,r3
  8c:	e8 83 01 22 	lhze    r4,18\(r3\)
  90:	e8 c9 01 43 	lhzue   r6,20\(r9\)
  94:	7c a7 4a 7e 	lhzuxe  r5,r7,r9
  98:	7d 27 2a 3e 	lhzxe   r9,r7,r5
  9c:	7d 4f a0 fc 	lwarxe  r10,r15,r20
  a0:	7c aa 94 3c 	lwbrxe  r5,r10,r18
  a4:	eb 9d 00 46 	lwze    r28,4\(r29\)
  a8:	e9 0a 02 87 	lwzue   r8,40\(r10\)
  ac:	7c 66 48 7e 	lwzuxe  r3,r6,r9
  b0:	7f dd e0 3e 	lwzxe   r30,r29,r28
  b4:	7c 06 3d fc 	dcbae   r6,r7
  b8:	7c 08 48 bc 	dcbfe   r8,r9
  bc:	7c 0a 5b bc 	dcbie   r10,r11
  c0:	7c 08 f0 7c 	dcbste  r8,r30
  c4:	7c c3 0a 3c 	dcbte   6,r3,r1
  c8:	7c a4 11 fa 	dcbtste 5,r4,r2
  cc:	7c 0f 77 fc 	dcbze   r15,r14
  d0:	7c 03 27 bc 	icbie   r3,r4
  d4:	7c a8 48 2c 	icbt    5,r8,r9
  d8:	7c ca 78 3c 	icbte   6,r10,r15
  dc:	7c a6 02 26 	mfapidi r5,r6
  e0:	7c 07 46 24 	tlbivax r7,r8
  e4:	7c 09 56 26 	tlbivaxe r9,r10
  e8:	7c 0b 67 24 	tlbsx   r11,r12
  ec:	7c 0d 77 26 	tlbsxe  r13,r14
  f0:	7c 22 1b 14 	adde64  r1,r2,r3
  f4:	7c 85 37 14 	adde64o r4,r5,r6
  f8:	7c e8 03 d4 	addme64 r7,r8
  fc:	7d 2a 07 d4 	addme64o r9,r10
 100:	7d 6c 03 94 	addze64 r11,r12
 104:	7d ae 07 94 	addze64o r13,r14
 108:	7e 80 04 40 	mcrxr64 cr5
 10c:	7d f0 8b 10 	subfe64 r15,r16,r17
 110:	7e 53 a7 10 	subfe64o r18,r19,r20
 114:	7e b6 03 d0 	subfme64 r21,r22
 118:	7e f8 07 d0 	subfme64o r23,r24
 11c:	7f 3a 03 90 	subfze64 r25,r26
 120:	7f 7c 07 90 	subfze64o r27,r28
 124:	e8 22 03 28 	stbe    r1,50\(r2\)
 128:	e8 64 02 89 	stbue   r3,40\(r4\)
 12c:	7c a6 39 fe 	stbuxe  r5,r6,r7
 130:	7d 09 51 be 	stbxe   r8,r9,r10
 134:	7d 6c 6b ff 	stdcxe. r11,r12,r13
 138:	f9 cf 00 78 	stde    r14,28\(r15\)
 13c:	fa 11 00 59 	stdue   r16,20\(r17\)
 140:	7e 53 a7 3e 	stdxe   r18,r19,r20
 144:	7e b6 bf 7e 	stduxe  r21,r22,r23
 148:	f8 38 00 3e 	stfde   f1,12\(r24\)
 14c:	f8 59 00 0f 	stfdue  f2,0\(r25\)
 150:	7c 7a dd be 	stfdxe  f3,r26,r27
 154:	7c 9c ed fe 	stfduxe f4,r28,r29
 158:	7c be ff be 	stfiwxe f5,r30,r31
 15c:	f8 de 00 6c 	stfse   f6,24\(r30\)
 160:	f8 fd 00 5d 	stfsue  f7,20\(r29\)
 164:	7d 1c dd 3e 	stfsxe  f8,r28,r27
 168:	7d 3a cd 7e 	stfsuxe f9,r26,r25
 16c:	7f 17 b7 3c 	sthbrxe r24,r23,r22
 170:	ea b4 01 ea 	sthe    r21,30\(r20\)
 174:	ea 72 02 8b 	sthue   r19,40\(r18\)
 178:	7e 30 7b 7e 	sthuxe  r17,r16,r15
 17c:	7d cd 63 3e 	sthxe   r14,r13,r12
 180:	7d 6a 4d 3c 	stwbrxe r11,r10,r9
 184:	7d 07 31 3d 	stwcxe. r8,r7,r6
 188:	e8 a4 03 2e 	stwe    r5,50\(r4\)
 18c:	e8 62 02 8f 	stwue   r3,40\(r2\)
 190:	7c 22 19 7e 	stwuxe  r1,r2,r3
 194:	7c 85 31 3e 	stwxe   r4,r5,r6
 198:	4c 00 00 66 	rfci
 19c:	7c 60 01 06 	wrtee   r3
 1a0:	7c 00 81 46 	wrteei  1
 1a4:	7c 85 02 06 	mfdcrx  r4,r5
 1a8:	7c aa 3a 86 	mfdcr   r5,234
 1ac:	7c e6 03 06 	mtdcrx  r6,r7
 1b0:	7d 10 6b 86 	mtdcr   432,r8
 1b4:	7c 00 04 ac 	sync    
 1b8:	7c 09 55 ec 	dcba    r9,r10
 1bc:	7c 00 06 ac 	eieio
