	.global entry
	.text
entry:
	and	r0, r0, r0
	ands	r0, r0, r0
	eor	r0, r0, r0
	eors	r0, r0, r0
	sub	r0, r0, r0
	subs	r0, r0, r0
	rsb	r0, r0, r0
	rsbs	r0, r0, r0
	add	r0, r0, r0
	adds	r0, r0, r0
	adc	r0, r0, r0
	adcs	r0, r0, r0
	sbc	r0, r0, r0
	sbcs	r0, r0, r0
	rsc	r0, r0, r0
	rscs	r0, r0, r0
	orr	r0, r0, r0
	orrs	r0, r0, r0
	bic	r0, r0, r0
	bics	r0, r0, r0

	tst	r0, r0
	tsts	r0, r0
	tstp	r0, r0
	teq	r0, r0
	teqs	r0, r0
	teqp	r0, r0
	cmp	r0, r0
	cmps	r0, r0
	cmpp	r0, r0
	cmn	r0, r0
	cmns	r0, r0
	cmnp	r0, r0

	mov	r0, r0
	movs	r0, r0
	mvn	r0, r0
	mvns	r0, r0

	swi	#0

	ldr	r0, [r0, #-0]
	ldrb	r0, [r0, #-0]
	ldrt	r0, [r1]
	ldrbt	r0, [r1]
	str	r0, [r0, #-0]
	strb	r0, [r0, #-0]
	strt	r0, [r1]
	strbt	r0, [r1]

	stmia	r0, {r0}
	stmib	r0, {r0}
	stmda	r0, {r0}
	stmdb	r0, {r0}
	stmfd	r0, {r0}
	stmfa	r0, {r0}
	stmea	r0, {r0}
	stmed	r0, {r0}

	ldmia	r0, {r0}
	ldmib	r0, {r0}
	ldmda	r0, {r0}
	ldmdb	r0, {r0}
	ldmfd	r0, {r0}
	ldmfa	r0, {r0}
	ldmea	r0, {r0}
	ldmed	r0, {r0}

	# Add three nop instructions to ensure that the
	# output is 32-byte aligned as required for arm-aout.
	nop
	nop
	nop
