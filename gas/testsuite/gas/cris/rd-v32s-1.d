#source: v32-err-1.s
#as: --underscore --em=criself --march=v32
#objdump: -dr

# Check that v32 insns that are expected to give syntax errors
# for non-v32 are recognized and resulting in correct code and
# disassembly.

.*:     file format elf32-us-cris

Disassembly of section \.text:

00000000 <here>:
   0:	6f3a                	move\.d \[acr\],r3
   2:	65fe                	move\.d \[r5\+\],acr
   4:	6f76                	move\.d acr,r7
   6:	68f6                	move\.d r8,acr
   8:	3fb6                	move acr,srp
   a:	7005                	addc r0,r0
   c:	7ff5                	addc acr,acr
   e:	7615                	addc r6,r1
  10:	a319                	addc \[r3\],r1
  12:	a009                	addc \[r0\],r0
  14:	aff9                	addc \[acr\],acr
  16:	af19                	addc \[acr\],r1
  18:	a31d                	addc \[r3\+\],r1
  1a:	5285                	addi r8\.w,r2,acr
  1c:	4005                	addi r0\.b,r0,acr
  1e:	6ff5                	addi acr\.d,acr,acr
  20:	6379                	addo\.d \[r3\],r7,acr
  22:	6d7d                	addo\.d \[r13\+\],r7,acr
  24:	63f9                	addo\.d \[r3\],acr,acr
  26:	4009                	addo\.b \[r0\],r0,acr
  28:	6ff9                	addo\.d \[acr\],acr,acr
  2a:	4ffd ffff           	addo\.b 0xffff,acr,acr
  2e:	5ffd ffff           	addo\.w 0xffff,acr,acr
  32:	6ffd ffff ffff      	addo\.d 0xffffffff,acr,acr
  38:	4f3d 0000           	addo\.b 0x0,r3,acr
			3a: R_CRIS_16	extsym1
  3c:	5f3d 0000           	addo\.w 0x0,r3,acr
			3e: R_CRIS_16	extsym2
  40:	6f3d 0000 0000      	addo\.d 0 <here>,r3,acr
			42: R_CRIS_32	extsym3
  46:	4ffd 7f00           	addo\.b 0x7f,acr,acr
  4a:	5ffd ff7f           	addo\.w 0x7fff,acr,acr
  4e:	6ffd ffff ff00      	addo\.d ffffff <here\+0xffffff>,acr,acr
  54:	4ffd 80ff           	addo\.b 0xff80,acr,acr
  58:	5ffd 0080           	addo\.w 0x8000,acr,acr
  5c:	6ffd ffff ffff      	addo\.d 0xffffffff,acr,acr
  62:	7009                	lapcq 62 <here\+0x62>,r0
  64:	7f49                	lapcq 82 <here\+0x82>,r4
  66:	7ff9                	lapcq 84 <here\+0x84>,acr
  68:	7ffd 0000 0000      	lapc 68 <here\+0x68>,acr
			6a: R_CRIS_32_PCREL	extsym4\+0x6
  6e:	7f4d 0000 0000      	lapc 6e <here\+0x6e>,r4
			70: R_CRIS_32_PCREL	extsym5\+0x6
  74:	7f4d 8cff ffff      	lapc 0 <here>,r4
  7a:	fff1                	addoq -1,acr,acr
  7c:	0001                	addoq 0,r0,acr
  7e:	7f41                	addoq 127,r4,acr
  80:	0041                	addoq 0,r4,acr
			80: R_CRIS_8	extsym6
  82:	bfbe 0000 0000      	bsr 82 <here\+0x82>
			84: R_CRIS_32_PCREL	\*ABS\*\+0x5
  88:	bf0e 0000 0000      	ba 88 <here\+0x88>
			8a: R_CRIS_32_PCREL	extsym7\+0x6
  8e:	bfae 72ff ffff      	bas 0 <here>,erp
  94:	ffbe 0000 0000      	bsrc 94 <here\+0x94>
			96: R_CRIS_32_PCREL	\*ABS\*\+0x5
  9a:	0000                	bcc \.
  9c:	0000                	bcc \.
  9e:	ff0e 0000 0000      	basc 9e <here\+0x9e>,bz
			a0: R_CRIS_32_PCREL	extsym8\+0x6
  a4:	0000                	bcc \.
  a6:	0000                	bcc \.
  a8:	ffae 58ff ffff      	basc 0 <here>,erp
  ae:	0000                	bcc \.
  b0:	0000                	bcc \.
  b2:	00f0                	bsb b2 <here\+0xb2>
  b4:	b005                	nop 
  b6:	4bf0                	bsb 0 <here>
  b8:	b005                	nop 
  ba:	bfbe 0000 0000      	bsr ba <here\+0xba>
			bc: R_CRIS_32_PCREL	extsym9\+0x6
  c0:	bfbe 40ff ffff      	bsr 0 <here>
  c6:	ffbe 0000 0000      	bsrc c6 <here\+0xc6>
			c8: R_CRIS_32_PCREL	\*ABS\*\+0x5
  cc:	0000                	bcc \.
  ce:	0000                	bcc \.
  d0:	ffbe 0000 0000      	bsrc d0 <here\+0xd0>
			d2: R_CRIS_32_PCREL	extsym10\+0x6
  d6:	0000                	bcc \.
  d8:	0000                	bcc \.
  da:	ffbe 26ff ffff      	bsrc 0 <here>
  e0:	0000                	bcc \.
  e2:	0000                	bcc \.
  e4:	b00a                	fidxd \[r0\]
  e6:	bf0a                	fidxd \[acr\]
  e8:	300d                	fidxi \[r0\]
  ea:	3f0d                	fidxi \[acr\]
  ec:	b01a                	ftagd \[r0\]
  ee:	bf1a                	ftagd \[acr\]
  f0:	301d                	ftagi \[r0\]
  f2:	3f1d                	ftagi \[acr\]
  f4:	b009                	jump r0
  f6:	bfe9                	jas acr,usp
  f8:	bf0d 0000 0000      	jump 0 <here>
			fa: R_CRIS_32	extsym9
  fe:	bfbd 0000 0000      	jsr 0 <here>
			100: R_CRIS_32	\.text
 104:	300b                	jasc r0,bz
 106:	0000                	bcc \.
 108:	0000                	bcc \.
 10a:	3feb                	jasc acr,usp
 10c:	0000                	bcc \.
 10e:	0000                	bcc \.
 110:	3fbf ffff ffff      	jsrc ffffffff <here\+0xffffffff>
 116:	0000                	bcc \.
 118:	0000                	bcc \.
 11a:	3f0f 0000 0000      	jasc 0 <here>,bz
			11c: R_CRIS_32	extsym11
 120:	0000                	bcc \.
 122:	0000                	bcc \.
 124:	3faf 0000 0000      	jasc 0 <here>,erp
			126: R_CRIS_32	\.text
 12a:	0000                	bcc \.
 12c:	0000                	bcc \.
 12e:	f0b9                	ret 
 130:	f009                	jump bz
 132:	f007                	mcp bz,r0
 134:	ff77                	mcp mof,acr
 136:	f2b7                	mcp srp,r2
 138:	700f                	move s0,r0
 13a:	7fff                	move s15,acr
 13c:	735f                	move s5,r3
 13e:	700b                	move r0,s0
 140:	7ffb                	move acr,s15
 142:	74ab                	move r4,s10
 144:	3029                	rfe 
 146:	3049                	rfg 
 148:	f0a9                	rete 
 14a:	f0c9                	retn 
 14c:	30f5                	ssb r0
 14e:	3ff5                	ssb acr
 150:	3af5                	ssb r10
 152:	3039                	sfe 
 154:	30f9                	halt 
 156:	3059                	rfn 
