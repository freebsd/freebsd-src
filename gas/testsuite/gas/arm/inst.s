@	Test file for ARM/GAS -- basic instructions

.text
.align
	mov	r0, #0
	mov	r1, r2
	mov	r3, r4, lsl #3
	mov	r5, r6, lsr r7
	mov	r8, r9, asr r10
	mov	r11, r12, asl r13
	mov	r14, r15, rrx
	moval	a2, a3
	moveq	a3, a4
	movne	v1, v2
	movlt	v3, v4
	movge	v5, v6
	movle	v7, v8
	movgt	ip, sp
	movcc	r1, r2
	movcs	r1, r3
	movmi	r3, r6
	movpl	wr, sb
	movvs	r1, r8
	movvc	SB, r1, lsr #31
	movhi	r8, pc
	movls	PC, lr
	movhs	r9, r8
	movul	r1, r3
	movs	r0, r8
	movuls	r0, WR
	
	add	r0, r1, #10
	add	r2, r3, r4
	add	r5, r6, r7, asl #5
	add	r1, r2, r3, lsl r1

	and	r0, r1, #10
	and	r2, r3, r4
	and	r5, r6, r7, asl #5
	and	r1, r2, r3, lsl r1

	eor	r0, r1, #10
	eor	r2, r3, r4
	eor	r5, r6, r7, asl #5
	eor	r1, r2, r3, lsl r1

	sub	r0, r1, #10
	sub	r2, r3, r4
	sub	r5, r6, r7, asl #5
	sub	r1, r2, r3, lsl r1

	adc	r0, r1, #10
	adc	r2, r3, r4
	adc	r5, r6, r7, asl #5
	adc	r1, r2, r3, lsl r1

	sbc	r0, r1, #10
	sbc	r2, r3, r4
	sbc	r5, r6, r7, asl #5
	sbc	r1, r2, r3, lsl r1

	rsb	r0, r1, #10
	rsb	r2, r3, r4
	rsb	r5, r6, r7, asl #5
	rsb	r1, r2, r3, lsl r1

	rsc	r0, r1, #10
	rsc	r2, r3, r4
	rsc	r5, r6, r7, asl #5
	rsc	r1, r2, r3, lsl r1

	orr	r0, r1, #10
	orr	r2, r3, r4
	orr	r5, r6, r7, asl #5
	orr	r1, r2, r3, lsl r1

	bic	r0, r1, #10
	bic	r2, r3, r4
	bic	r5, r6, r7, asl #5
	bic	r1, r2, r3, lsl r1

	mvn	r0, #10
	mvn	r2, r4
	mvn	r5, r7, asl #5
	mvn	r1, r3, lsl r1

	tst	r0, #10
	tst	r2, r4
	tst	r5, r7, asl #5
	tst	r1, r3, lsl r1

	teq	r0, #10
	teq	r2, r4
	teq	r5, r7, asl #5
	teq	r1, r3, lsl r1

	cmp	r0, #10
	cmp	r2, r4
	cmp	r5, r7, asl #5
	cmp	r1, r3, lsl r1

	cmn	r0, #10
	cmn	r2, r4
	cmn	r5, r7, asl #5
	cmn	r1, r3, lsl r1

	teqp	r0, #10
	teqp	r2, r4
	teqp	r5, r7, asl #5
	teqp	r1, r3, lsl r1

	cmnp	r0, #10
	cmnp	r2, r4
	cmnp	r5, r7, asl #5
	cmnp	r1, r3, lsl r1

	cmpp	r0, #10
	cmpp	r2, r4
	cmpp	r5, r7, asl #5
	cmpp	r1, r3, lsl r1

	tstp	r0, #10
	tstp	r2, r4
	tstp	r5, r7, asl #5
	tstp	r1, r3, lsl r1

	mul	r0, r1, r2
	muls	r1, r2, r3
	mulne	r0, r1, r0
	mullss	r9, r8, r7

	mla	r1, r9, sl, fp
	mlas	r3, r4, r9, IP
	mlalt	r9, r8, r7, SP
	mlages	r4, r1, r3, LR

	ldr	r0, [r1]
	ldr	r1, [r1, r2]
	ldr	r2, [r3, r4]!
	ldr	r2, [r2, #32]
	ldr	r2, [r3, r4, lsr #8]
	ldreq	r4, [r5, r4, asl #9]!
	ldrne	r4, [r5], #6
	ldrt	r1, [r2], r3
	ldr	r2, [r4], r5, lsr #8
foo:
	ldr	r0, foo
	ldrb	r3, [r4]
	ldrnebt	r5, [r8]
	
	str	r0, [r1]
	str	r1, [r1, r2]
	str	r3, [r4, r3]!
	str	r2, [r2, #32]
	str	r2, [r3, r4, lsr #8]
	streq	r4, [r5, r4, asl #9]!
	strne	r4, [r5], #6
	str	r1, [r2], r3
	strt	r2, [r4], r5, lsr #8
	str	r1, bar
bar:
	stralb	r1, [r7]
	strbt	r2, [r0]

	ldmia	r0, {r1}
	ldmeqib	r2, {r3, r4, r5}
	ldmalda	r3, {r0-r15}^
	ldmdb	FP!, {r0-r8, SL}
	ldmed	r1, {r0, r1, r2}|0xf0
	ldmfd	r2, {r3, r4}+{r5, r6, r7, r8}
	ldmea	r3, 3
	ldmfa	r4, {r8, r9}^
	
	stmia	r0, {r1}
	stmeqib	r2, {r3, r4, r5}
	stmalda	r3, {r0-r15}^
	stmdb	r11!, {r0-r8, r10}
	stmed	r1, {r0, r1, r2}
	stmfd	r2, {r3, r4}
	stmea	r3, 3
	stmfa	r4, {r8, r9}^

	swi	0x123456
	swihs	0x33

	bl	_wombat
	blpl	hohum
	b	_wibble
	ble	testerfunc

	mov r1, r2, lsl #2
	mov r1, r2, lsl #0 
	mov r1, r2, lsl #31
	mov r1, r2, lsl r3
	mov r1, r2, lsr #2
	mov r1, r2, lsr #31
	mov r1, r2, lsr #32
	mov r1, r2, lsr r3
	mov r1, r2, asr #2
	mov r1, r2, asr #31
	mov r1, r2, asr #32
	mov r1, r2, asr r3
	mov r1, r2, ror #2
	mov r1, r2, ror #31
	mov r1, r2, ror r3
	mov r1, r2, rrx
	mov r1, r2, LSL #2
	mov r1, r2, LSL #0 
	mov r1, r2, LSL #31
	mov r1, r2, LSL r3
	mov r1, r2, LSR #2
	mov r1, r2, LSR #31
	mov r1, r2, LSR #32
	mov r1, r2, LSR r3
	mov r1, r2, ASR #2
	mov r1, r2, ASR #31
	mov r1, r2, ASR #32
	mov r1, r2, ASR r3
	mov r1, r2, ROR #2
	mov r1, r2, ROR #31
	mov r1, r2, ROR r3
	mov r1, r2, RRX
	