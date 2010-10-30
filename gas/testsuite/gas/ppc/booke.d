#as: -mbooke64
#objdump: -dr -Mbooke
#name: BookE tests

.*: +file format elf(32)?(64)?-powerpc.*

Disassembly of section \.text:

0+0000000 <start>:
   0:	24 25 00 30 	bce     1,4\*cr1\+gt,30 <branch_target_1>
   4:	24 46 00 3d 	bcel    2,4\*cr1\+eq,40 <branch_target_2>
   8:	24 67 00 52 	bcea    3,4\*cr1\+so,50 <branch_target_3>
			8: R_PPC(64)?_ADDR14	\.text\+0x50
   c:	24 88 00 73 	bcela   4,4\*cr2\+lt,70 <branch_target_4>
			c: R_PPC(64)?_ADDR14	\.text\+0x70
  10:	4c a9 00 22 	bclre   5,4\*cr2\+gt
  14:	4c aa 00 23 	bclrel  5,4\*cr2\+eq
  18:	4d 0b 04 22 	bcctre  8,4\*cr2\+so
  1c:	4d 0c 04 23 	bcctrel 8,4\*cr3\+lt
  20:	58 00 00 74 	be      94 <branch_target_5>
  24:	58 00 00 89 	bel     ac <branch_target_6>
  28:	58 00 00 f6 	bea     f4 <branch_target_7>
			28: R_PPC(64)?_ADDR24	\.text\+0xf4
  2c:	58 00 01 2b 	bela    128 <branch_target_8>
			2c: R_PPC(64)?_ADDR24	\.text\+0x128

0+0000030 <branch_target_1>:
  30:	e9 09 00 80 	lbze    r8,8\(r9\)
  34:	e9 8f 00 41 	lbzue   r12,4\(r15\)
  38:	7c 86 40 fe 	lbzuxe  r4,r6,r8
  3c:	7c 65 38 be 	lbzxe   r3,r5,r7

0+0000040 <branch_target_2>:
  40:	f8 a6 06 40 	lde     r5,400\(r6\)
  44:	f8 c7 07 11 	ldue    r6,452\(r7\)
  48:	7c e8 4e 3e 	ldxe    r7,r8,r9
  4c:	7d 4b 66 7e 	lduxe   r10,r11,r12

0+0000050 <branch_target_3>:
  50:	f9 81 02 06 	lfde    f12,128\(r1\)
  54:	f8 25 00 47 	lfdue   f1,16\(r5\)
  58:	7c a1 1c be 	lfdxe   f5,r1,r3
  5c:	7c c2 24 fe 	lfduxe  f6,r2,r4
  60:	f9 09 00 c4 	lfse    f8,48\(r9\)
  64:	f9 2a 01 15 	lfsue   f9,68\(r10\)
  68:	7d 44 44 7e 	lfsuxe  f10,r4,r8
  6c:	7d 23 3c 3e 	lfsxe   f9,r3,r7

0+0000070 <branch_target_4>:
  70:	e9 45 03 24 	lhae    r10,50\(r5\)
  74:	e8 23 00 55 	lhaue   r1,5\(r3\)
  78:	7c a1 1a fe 	lhauxe  r5,r1,r3
  7c:	7f be fa be 	lhaxe   r29,r30,r31
  80:	7c 22 1e 3c 	lhbrxe  r1,r2,r3
  84:	e8 83 01 22 	lhze    r4,18\(r3\)
  88:	e8 c9 01 43 	lhzue   r6,20\(r9\)
  8c:	7c a7 4a 7e 	lhzuxe  r5,r7,r9
  90:	7d 27 2a 3e 	lhzxe   r9,r7,r5

0+0000094 <branch_target_5>:
  94:	7d 4f a0 fc 	lwarxe  r10,r15,r20
  98:	7c aa 94 3c 	lwbrxe  r5,r10,r18
  9c:	eb 9d 00 46 	lwze    r28,4\(r29\)
  a0:	e9 0a 02 87 	lwzue   r8,40\(r10\)
  a4:	7c 66 48 7e 	lwzuxe  r3,r6,r9
  a8:	7f dd e0 3e 	lwzxe   r30,r29,r28

0+00000ac <branch_target_6>:
  ac:	7c 06 3d fc 	dcbae   r6,r7
  b0:	7c 08 48 bc 	dcbfe   r8,r9
  b4:	7c 0a 5b bc 	dcbie   r10,r11
  b8:	7c 08 f0 7c 	dcbste  r8,r30
  bc:	7c c3 0a 3c 	dcbte   6,r3,r1
  c0:	7c a4 11 fa 	dcbtste 5,r4,r2
  c4:	7c 0f 77 fc 	dcbze   r15,r14
  c8:	7c 03 27 bc 	icbie   r3,r4
  cc:	7c a8 48 2c 	icbt    5,r8,r9
  d0:	7c ca 78 3c 	icbte   6,r10,r15
  d4:	7c a6 02 26 	mfapidi r5,r6
  d8:	7c 07 46 24 	tlbivax r7,r8
  dc:	7c 09 56 26 	tlbivaxe r9,r10
  e0:	7c 0b 67 24 	tlbsx   r11,r12
  e4:	7c 0d 77 26 	tlbsxe  r13,r14
  e8:	7c 00 07 a4 	tlbwe   
  ec:	7c 00 07 a4 	tlbwe   
  f0:	7c 21 0f a4 	tlbwe   r1,r1,1

0+00000f4 <branch_target_7>:
  f4:	7c 22 1b 14 	adde64  r1,r2,r3
  f8:	7c 85 37 14 	adde64o r4,r5,r6
  fc:	7c e8 03 d4 	addme64 r7,r8
 100:	7d 2a 07 d4 	addme64o r9,r10
 104:	7d 6c 03 94 	addze64 r11,r12
 108:	7d ae 07 94 	addze64o r13,r14
 10c:	7e 80 04 40 	mcrxr64 cr5
 110:	7d f0 8b 10 	subfe64 r15,r16,r17
 114:	7e 53 a7 10 	subfe64o r18,r19,r20
 118:	7e b6 03 d0 	subfme64 r21,r22
 11c:	7e f8 07 d0 	subfme64o r23,r24
 120:	7f 3a 03 90 	subfze64 r25,r26
 124:	7f 7c 07 90 	subfze64o r27,r28

0+0000128 <branch_target_8>:
 128:	e8 22 03 28 	stbe    r1,50\(r2\)
 12c:	e8 64 02 89 	stbue   r3,40\(r4\)
 130:	7c a6 39 fe 	stbuxe  r5,r6,r7
 134:	7d 09 51 be 	stbxe   r8,r9,r10
 138:	7d 6c 6b ff 	stdcxe\. r11,r12,r13
 13c:	f9 cf 00 78 	stde    r14,28\(r15\)
 140:	fa 11 00 59 	stdue   r16,20\(r17\)
 144:	7e 53 a7 3e 	stdxe   r18,r19,r20
 148:	7e b6 bf 7e 	stduxe  r21,r22,r23
 14c:	f8 38 00 3e 	stfde   f1,12\(r24\)
 150:	f8 59 00 0f 	stfdue  f2,0\(r25\)
 154:	7c 7a dd be 	stfdxe  f3,r26,r27
 158:	7c 9c ed fe 	stfduxe f4,r28,r29
 15c:	7c be ff be 	stfiwxe f5,r30,r31
 160:	f8 de 00 6c 	stfse   f6,24\(r30\)
 164:	f8 fd 00 5d 	stfsue  f7,20\(r29\)
 168:	7d 1c dd 3e 	stfsxe  f8,r28,r27
 16c:	7d 3a cd 7e 	stfsuxe f9,r26,r25
 170:	7f 17 b7 3c 	sthbrxe r24,r23,r22
 174:	ea b4 01 ea 	sthe    r21,30\(r20\)
 178:	ea 72 02 8b 	sthue   r19,40\(r18\)
 17c:	7e 30 7b 7e 	sthuxe  r17,r16,r15
 180:	7d cd 63 3e 	sthxe   r14,r13,r12
 184:	7d 6a 4d 3c 	stwbrxe r11,r10,r9
 188:	7d 07 31 3d 	stwcxe\. r8,r7,r6
 18c:	e8 a4 03 2e 	stwe    r5,50\(r4\)
 190:	e8 62 02 8f 	stwue   r3,40\(r2\)
 194:	7c 22 19 7e 	stwuxe  r1,r2,r3
 198:	7c 85 31 3e 	stwxe   r4,r5,r6
 19c:	4c 00 00 66 	rfci
 1a0:	7c 60 01 06 	wrtee   r3
 1a4:	7c 00 81 46 	wrteei  1
 1a8:	7c 85 02 06 	mfdcrx  r4,r5
 1ac:	7c aa 3a 86 	mfdcr   r5,234
 1b0:	7c e6 03 06 	mtdcrx  r6,r7
 1b4:	7d 10 6b 86 	mtdcr   432,r8
 1b8:	7c 00 04 ac 	msync
 1bc:	7c 09 55 ec 	dcba    r9,r10
 1c0:	7c 00 06 ac 	mbar    
 1c4:	7c 00 06 ac 	mbar    
 1c8:	7c 20 06 ac 	mbar    1
 1cc:	7d 8d 77 24 	tlbsx   r12,r13,r14
 1d0:	7d 8d 77 25 	tlbsx\.  r12,r13,r14
 1d4:	7d 8d 77 26 	tlbsxe  r12,r13,r14
 1d8:	7d 8d 77 27 	tlbsxe\. r12,r13,r14
 1dc:	7c 12 42 a6 	mfsprg  r0,2
 1e0:	7c 12 42 a6 	mfsprg  r0,2
 1e4:	7c 12 43 a6 	mtsprg  2,r0
 1e8:	7c 12 43 a6 	mtsprg  2,r0
 1ec:	7c 07 42 a6 	mfsprg  r0,7
 1f0:	7c 07 42 a6 	mfsprg  r0,7
 1f4:	7c 17 43 a6 	mtsprg  7,r0
 1f8:	7c 17 43 a6 	mtsprg  7,r0
