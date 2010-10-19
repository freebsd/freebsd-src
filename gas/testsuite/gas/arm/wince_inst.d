#objdump: -dr --prefix-addresses --show-raw-insn
#name: ARM basic instructions (WinCE version)
#as: -mcpu=arm7m -EL
#source: inst.s
# inst.d is used for non-WinCE targets.
#not-skip: *-wince-*

# This file is the same as inst.d except that the BL
# instructions have not had a -8 bias inserted.

.*: +file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> e3a00000 ?	mov	r0, #0	; 0x0
0+004 <[^>]*> e1a01002 ?	mov	r1, r2
0+008 <[^>]*> e1a03184 ?	mov	r3, r4, lsl #3
0+00c <[^>]*> e1a05736 ?	mov	r5, r6, lsr r7
0+010 <[^>]*> e1a08a59 ?	mov	r8, r9, asr sl
0+014 <[^>]*> e1a0bd1c ?	mov	fp, ip, lsl sp
0+018 <[^>]*> e1a0e06f ?	mov	lr, pc, rrx
0+01c <[^>]*> e1a01002 ?	mov	r1, r2
0+020 <[^>]*> 01a02003 ?	moveq	r2, r3
0+024 <[^>]*> 11a04005 ?	movne	r4, r5
0+028 <[^>]*> b1a06007 ?	movlt	r6, r7
0+02c <[^>]*> a1a08009 ?	movge	r8, r9
0+030 <[^>]*> d1a0a00b ?	movle	sl, fp
0+034 <[^>]*> c1a0c00d ?	movgt	ip, sp
0+038 <[^>]*> 31a01002 ?	movcc	r1, r2
0+03c <[^>]*> 21a01003 ?	movcs	r1, r3
0+040 <[^>]*> 41a03006 ?	movmi	r3, r6
0+044 <[^>]*> 51a07009 ?	movpl	r7, r9
0+048 <[^>]*> 61a01008 ?	movvs	r1, r8
0+04c <[^>]*> 71a09fa1 ?	movvc	r9, r1, lsr #31
0+050 <[^>]*> 81a0800f ?	movhi	r8, pc
0+054 <[^>]*> 91a0f00e ?	movls	pc, lr
0+058 <[^>]*> 21a09008 ?	movcs	r9, r8
0+05c <[^>]*> 31a01003 ?	movcc	r1, r3
0+060 <[^>]*> e1b00008 ?	movs	r0, r8
0+064 <[^>]*> 31b00007 ?	movccs	r0, r7
0+068 <[^>]*> e281000a ?	add	r0, r1, #10	; 0xa
0+06c <[^>]*> e0832004 ?	add	r2, r3, r4
0+070 <[^>]*> e0865287 ?	add	r5, r6, r7, lsl #5
0+074 <[^>]*> e0821113 ?	add	r1, r2, r3, lsl r1
0+078 <[^>]*> e201000a ?	and	r0, r1, #10	; 0xa
0+07c <[^>]*> e0032004 ?	and	r2, r3, r4
0+080 <[^>]*> e0065287 ?	and	r5, r6, r7, lsl #5
0+084 <[^>]*> e0021113 ?	and	r1, r2, r3, lsl r1
0+088 <[^>]*> e221000a ?	eor	r0, r1, #10	; 0xa
0+08c <[^>]*> e0232004 ?	eor	r2, r3, r4
0+090 <[^>]*> e0265287 ?	eor	r5, r6, r7, lsl #5
0+094 <[^>]*> e0221113 ?	eor	r1, r2, r3, lsl r1
0+098 <[^>]*> e241000a ?	sub	r0, r1, #10	; 0xa
0+09c <[^>]*> e0432004 ?	sub	r2, r3, r4
0+0a0 <[^>]*> e0465287 ?	sub	r5, r6, r7, lsl #5
0+0a4 <[^>]*> e0421113 ?	sub	r1, r2, r3, lsl r1
0+0a8 <[^>]*> e2a1000a ?	adc	r0, r1, #10	; 0xa
0+0ac <[^>]*> e0a32004 ?	adc	r2, r3, r4
0+0b0 <[^>]*> e0a65287 ?	adc	r5, r6, r7, lsl #5
0+0b4 <[^>]*> e0a21113 ?	adc	r1, r2, r3, lsl r1
0+0b8 <[^>]*> e2c1000a ?	sbc	r0, r1, #10	; 0xa
0+0bc <[^>]*> e0c32004 ?	sbc	r2, r3, r4
0+0c0 <[^>]*> e0c65287 ?	sbc	r5, r6, r7, lsl #5
0+0c4 <[^>]*> e0c21113 ?	sbc	r1, r2, r3, lsl r1
0+0c8 <[^>]*> e261000a ?	rsb	r0, r1, #10	; 0xa
0+0cc <[^>]*> e0632004 ?	rsb	r2, r3, r4
0+0d0 <[^>]*> e0665287 ?	rsb	r5, r6, r7, lsl #5
0+0d4 <[^>]*> e0621113 ?	rsb	r1, r2, r3, lsl r1
0+0d8 <[^>]*> e2e1000a ?	rsc	r0, r1, #10	; 0xa
0+0dc <[^>]*> e0e32004 ?	rsc	r2, r3, r4
0+0e0 <[^>]*> e0e65287 ?	rsc	r5, r6, r7, lsl #5
0+0e4 <[^>]*> e0e21113 ?	rsc	r1, r2, r3, lsl r1
0+0e8 <[^>]*> e381000a ?	orr	r0, r1, #10	; 0xa
0+0ec <[^>]*> e1832004 ?	orr	r2, r3, r4
0+0f0 <[^>]*> e1865287 ?	orr	r5, r6, r7, lsl #5
0+0f4 <[^>]*> e1821113 ?	orr	r1, r2, r3, lsl r1
0+0f8 <[^>]*> e3c1000a ?	bic	r0, r1, #10	; 0xa
0+0fc <[^>]*> e1c32004 ?	bic	r2, r3, r4
0+100 <[^>]*> e1c65287 ?	bic	r5, r6, r7, lsl #5
0+104 <[^>]*> e1c21113 ?	bic	r1, r2, r3, lsl r1
0+108 <[^>]*> e3e0000a ?	mvn	r0, #10	; 0xa
0+10c <[^>]*> e1e02004 ?	mvn	r2, r4
0+110 <[^>]*> e1e05287 ?	mvn	r5, r7, lsl #5
0+114 <[^>]*> e1e01113 ?	mvn	r1, r3, lsl r1
0+118 <[^>]*> e310000a ?	tst	r0, #10	; 0xa
0+11c <[^>]*> e1120004 ?	tst	r2, r4
0+120 <[^>]*> e1150287 ?	tst	r5, r7, lsl #5
0+124 <[^>]*> e1110113 ?	tst	r1, r3, lsl r1
0+128 <[^>]*> e330000a ?	teq	r0, #10	; 0xa
0+12c <[^>]*> e1320004 ?	teq	r2, r4
0+130 <[^>]*> e1350287 ?	teq	r5, r7, lsl #5
0+134 <[^>]*> e1310113 ?	teq	r1, r3, lsl r1
0+138 <[^>]*> e350000a ?	cmp	r0, #10	; 0xa
0+13c <[^>]*> e1520004 ?	cmp	r2, r4
0+140 <[^>]*> e1550287 ?	cmp	r5, r7, lsl #5
0+144 <[^>]*> e1510113 ?	cmp	r1, r3, lsl r1
0+148 <[^>]*> e370000a ?	cmn	r0, #10	; 0xa
0+14c <[^>]*> e1720004 ?	cmn	r2, r4
0+150 <[^>]*> e1750287 ?	cmn	r5, r7, lsl #5
0+154 <[^>]*> e1710113 ?	cmn	r1, r3, lsl r1
0+158 <[^>]*> e330f00a ?	teqp	r0, #10	; 0xa
0+15c <[^>]*> e132f004 ?	teqp	r2, r4
0+160 <[^>]*> e135f287 ?	teqp	r5, r7, lsl #5
0+164 <[^>]*> e131f113 ?	teqp	r1, r3, lsl r1
0+168 <[^>]*> e370f00a ?	cmnp	r0, #10	; 0xa
0+16c <[^>]*> e172f004 ?	cmnp	r2, r4
0+170 <[^>]*> e175f287 ?	cmnp	r5, r7, lsl #5
0+174 <[^>]*> e171f113 ?	cmnp	r1, r3, lsl r1
0+178 <[^>]*> e350f00a ?	cmpp	r0, #10	; 0xa
0+17c <[^>]*> e152f004 ?	cmpp	r2, r4
0+180 <[^>]*> e155f287 ?	cmpp	r5, r7, lsl #5
0+184 <[^>]*> e151f113 ?	cmpp	r1, r3, lsl r1
0+188 <[^>]*> e310f00a ?	tstp	r0, #10	; 0xa
0+18c <[^>]*> e112f004 ?	tstp	r2, r4
0+190 <[^>]*> e115f287 ?	tstp	r5, r7, lsl #5
0+194 <[^>]*> e111f113 ?	tstp	r1, r3, lsl r1
0+198 <[^>]*> e0000291 ?	mul	r0, r1, r2
0+19c <[^>]*> e0110392 ?	muls	r1, r2, r3
0+1a0 <[^>]*> 10000091 ?	mulne	r0, r1, r0
0+1a4 <[^>]*> 90190798 ?	mullss	r9, r8, r7
0+1a8 <[^>]*> e021ba99 ?	mla	r1, r9, sl, fp
0+1ac <[^>]*> e033c994 ?	mlas	r3, r4, r9, ip
0+1b0 <[^>]*> b029d798 ?	mlalt	r9, r8, r7, sp
0+1b4 <[^>]*> a034e391 ?	mlages	r4, r1, r3, lr
0+1b8 <[^>]*> e5910000 ?	ldr	r0, \[r1\]
0+1bc <[^>]*> e7911002 ?	ldr	r1, \[r1, r2\]
0+1c0 <[^>]*> e7b32004 ?	ldr	r2, \[r3, r4\]!
0+1c4 <[^>]*> e5922020 ?	ldr	r2, \[r2, #32\]
0+1c8 <[^>]*> e7932424 ?	ldr	r2, \[r3, r4, lsr #8\]
0+1cc <[^>]*> 07b54484 ?	ldreq	r4, \[r5, r4, lsl #9\]!
0+1d0 <[^>]*> 14954006 ?	ldrne	r4, \[r5\], #6
0+1d4 <[^>]*> e6b21003 ?	ldrt	r1, \[r2\], r3
0+1d8 <[^>]*> e6942425 ?	ldr	r2, \[r4\], r5, lsr #8
0+1dc <[^>]*> e51f0008 ?	ldr	r0, \[pc, #-8\]	; 0+1dc <[^>]*>
0+1e0 <[^>]*> e5d43000 ?	ldrb	r3, \[r4\]
0+1e4 <[^>]*> 14f85000 ?	ldrnebt	r5, \[r8\]
0+1e8 <[^>]*> e5810000 ?	str	r0, \[r1\]
0+1ec <[^>]*> e7811002 ?	str	r1, \[r1, r2\]
0+1f0 <[^>]*> e7a43003 ?	str	r3, \[r4, r3\]!
0+1f4 <[^>]*> e5822020 ?	str	r2, \[r2, #32\]
0+1f8 <[^>]*> e7832424 ?	str	r2, \[r3, r4, lsr #8\]
0+1fc <[^>]*> 07a54484 ?	streq	r4, \[r5, r4, lsl #9\]!
0+200 <[^>]*> 14854006 ?	strne	r4, \[r5\], #6
0+204 <[^>]*> e6821003 ?	str	r1, \[r2\], r3
0+208 <[^>]*> e6a42425 ?	strt	r2, \[r4\], r5, lsr #8
0+20c <[^>]*> e50f1004 ?	str	r1, \[pc, #-4\]	; 0+210 <[^>]*>
0+210 <[^>]*> e5c71000 ?	strb	r1, \[r7\]
0+214 <[^>]*> e4e02000 ?	strbt	r2, \[r0\]
0+218 <[^>]*> e8900002 ?	ldmia	r0, {r1}
0+21c <[^>]*> 09920038 ?	ldmeqib	r2, {r3, r4, r5}
0+220 <[^>]*> e853ffff ?	ldmda	r3, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp, ip, sp, lr, pc}\^
0+224 <[^>]*> e93b05ff ?	ldmdb	fp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, sl}
0+228 <[^>]*> e99100f7 ?	ldmib	r1, {r0, r1, r2, r4, r5, r6, r7}
0+22c <[^>]*> e89201f8 ?	ldmia	r2, {r3, r4, r5, r6, r7, r8}
0+230 <[^>]*> e9130003 ?	ldmdb	r3, {r0, r1}
0+234 <[^>]*> e8540300 ?	ldmda	r4, {r8, r9}\^
0+238 <[^>]*> e8800002 ?	stmia	r0, {r1}
0+23c <[^>]*> 09820038 ?	stmeqib	r2, {r3, r4, r5}
0+240 <[^>]*> e843ffff ?	stmda	r3, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, sl, fp, ip, sp, lr, pc}\^
0+244 <[^>]*> e92b05ff ?	stmdb	fp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, sl}
0+248 <[^>]*> e8010007 ?	stmda	r1, {r0, r1, r2}
0+24c <[^>]*> e9020018 ?	stmdb	r2, {r3, r4}
0+250 <[^>]*> e8830003 ?	stmia	r3, {r0, r1}
0+254 <[^>]*> e9c40300 ?	stmib	r4, {r8, r9}\^
0+258 <[^>]*> ef123456 ?	(swi|svc)	0x00123456
0+25c <[^>]*> 2f000033 ?	(swi|svc)cs	0x00000033
0+260 <[^>]*> eb000000 ?	bl	0+268 <[^>]*>
[		]*260:.*_wombat.*
0+264 <[^>]*> 5b000000 ?	blpl	0+26c <[^>]*>
[		]*264:.*ARM.*hohum
0+268 <[^>]*> ea000000 ?	b	0+270 <[^>]*>
[		]*268:.*_wibble.*
0+26c <[^>]*> da000000 ?	ble	0+274 <[^>]*>
[		]*26c:.*testerfunc.*
0+270 <[^>]*> e1a01102 ?	mov	r1, r2, lsl #2
0+274 <[^>]*> e1a01002 ?	mov	r1, r2
0+278 <[^>]*> e1a01f82 ?	mov	r1, r2, lsl #31
0+27c <[^>]*> e1a01312 ?	mov	r1, r2, lsl r3
0+280 <[^>]*> e1a01122 ?	mov	r1, r2, lsr #2
0+284 <[^>]*> e1a01fa2 ?	mov	r1, r2, lsr #31
0+288 <[^>]*> e1a01022 ?	mov	r1, r2, lsr #32
0+28c <[^>]*> e1a01332 ?	mov	r1, r2, lsr r3
0+290 <[^>]*> e1a01142 ?	mov	r1, r2, asr #2
0+294 <[^>]*> e1a01fc2 ?	mov	r1, r2, asr #31
0+298 <[^>]*> e1a01042 ?	mov	r1, r2, asr #32
0+29c <[^>]*> e1a01352 ?	mov	r1, r2, asr r3
0+2a0 <[^>]*> e1a01162 ?	mov	r1, r2, ror #2
0+2a4 <[^>]*> e1a01fe2 ?	mov	r1, r2, ror #31
0+2a8 <[^>]*> e1a01372 ?	mov	r1, r2, ror r3
0+2ac <[^>]*> e1a01062 ?	mov	r1, r2, rrx
0+2b0 <[^>]*> e1a01102 ?	mov	r1, r2, lsl #2
0+2b4 <[^>]*> e1a01002 ?	mov	r1, r2
0+2b8 <[^>]*> e1a01f82 ?	mov	r1, r2, lsl #31
0+2bc <[^>]*> e1a01312 ?	mov	r1, r2, lsl r3
0+2c0 <[^>]*> e1a01122 ?	mov	r1, r2, lsr #2
0+2c4 <[^>]*> e1a01fa2 ?	mov	r1, r2, lsr #31
0+2c8 <[^>]*> e1a01022 ?	mov	r1, r2, lsr #32
0+2cc <[^>]*> e1a01332 ?	mov	r1, r2, lsr r3
0+2d0 <[^>]*> e1a01142 ?	mov	r1, r2, asr #2
0+2d4 <[^>]*> e1a01fc2 ?	mov	r1, r2, asr #31
0+2d8 <[^>]*> e1a01042 ?	mov	r1, r2, asr #32
0+2dc <[^>]*> e1a01352 ?	mov	r1, r2, asr r3
0+2e0 <[^>]*> e1a01162 ?	mov	r1, r2, ror #2
0+2e4 <[^>]*> e1a01fe2 ?	mov	r1, r2, ror #31
0+2e8 <[^>]*> e1a01372 ?	mov	r1, r2, ror r3
0+2ec <[^>]*> e1a01062 ?	mov	r1, r2, rrx
