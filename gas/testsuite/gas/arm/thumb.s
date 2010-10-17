	.text
	.code 16
.foo:	
	lsl	r2, r1, #3
	lsr	r3, r4, #31
wibble/data:	
	asr	r7, r0, #5

	lsl	r1, r2, #0
	lsr	r3, r4, #0
	asr	r4, r5, #0

	lsr	r6, r7, #32
	asr	r0, r1, #32

	add	r1, r2, r3
	add	r2, r4, #2
	sub	r3, r5, r7
	sub	r2, r4, #7

	mov	r4, #255
	cmp	r3, #250
	add	r6, #123
	sub	r5, #128

	and	r3, r5
	eor	r4, r6
	lsl	r1, r0
	lsr	r2, r3
	asr	r4, r6
	adc	r5, r7
	sbc	r0, r4
	ror	r1, r4
	tst	r2, r5
	neg	r1, r1
	cmp	r2, r3
	cmn	r1, r4
	orr	r0, r3
	mul	r4, r5
	bic	r5, r7
	mvn	r5, r5

	add	r1, r13
	add	r12, r2
	add	r9, r9
	cmp	r1, r14
	cmp	r8, r0
	cmp	r12, r14
	mov	r0, r9
	mov	r9, r4
	mov	r8, r8
	bx	r7
	bx	r8
	.align 0
	bx	pc

	ldr	r3, [pc, #128]
	ldr	r4, bar

	str	r0, [r1, r2]
	strb	r1, [r2, r4]
	ldr	r5, [r6, r7]
	ldrb	r2, [r4, r5]
	
	.align 0
bar:	
	strh	r1, [r2, r3]
	ldrh	r3, [r4, r0]
	ldsb	r1, [r6, r7]
	ldsh	r2, [r0, r5]

	str	r3, [r3, #124]
	ldr	r1, [r4, #124]
	ldr	r5, [r5]
	strb	r1, [r5, #31]
	strb	r1, [r4, #5]
	strb	r2, [r6]

	strh	r4, [r5, #62]
	ldrh	r5, [r0, #4]
	ldrh	r3, [r2]

	str	r3, [r13, #1020]
	ldr	r1, [r13, #44]
	ldr	r2, [r13]

	add	r7, r15, #1020
	add	r4, r13, #512

	add	r13, #268
	add	r13, #-104
	sub	r13, #268
	sub	r13, #-108

	push	{r0, r1, r2, r4}
	push	{r0, r3-r7, lr}
	pop	{r3, r4, r7}
	pop	{r0-r7, r15}

	stmia	r3!, {r0, r1, r4-r7}
	ldmia	r0!, {r1-r7}

	beq	bar
	bne	bar
	bcs	bar
	bcc	bar
	bmi	bar
	bpl	bar
	bvs	bar
	bvc	bar
	bhi	bar
	bls	bar
	bge	bar
	bgt	bar
	blt	bar
	bgt	bar
	ble	bar
	bhi	bar
	blo	bar
	bul	bar
	bal	bar

close:
	lsl	r4, r5, #near - close
near:
	add	r2, r3, #near - close

	add	sp, sp, #127 << 2
	sub	sp, sp, #127 << 2
	add	r0, sp, #255 << 2
	add	r0, pc, #255 << 2

	add	sp, sp, #bar - .foo
	sub	sp, sp, #bar - .foo
	add	r0, sp, #bar - .foo
	add	r0, pc, #bar - .foo

	add	r1, #bar - .foo
	mov	r6, #bar - .foo
	cmp	r7, #bar - .foo

	nop
	nop

	.arm
.localbar:
	b	.localbar
	b	.wombat
	bl	.localbar
	bl	.wombat

	bx	r0
	swi	0x123456

	.thumb
	@ The following will be disassembled incorrectly if we do not
	@ have a Thumb symbol defined before the first Thumb instruction:
morethumb:
	adr	r0, forwardonly

	b	.foo
	b	.wombat
	bl	.foo
	bl	.wombat

	bx	r0

	swi	0xff
	.align	0
forwardonly:
	beq	.wombat
	bne	.wombat
	bcs	.wombat
	bcc	.wombat
	bmi	.wombat
	bpl	.wombat
	bvs	.wombat
	bvc	.wombat
	bhi	.wombat
	bls	.wombat
	bge	.wombat
	bgt	.wombat
	blt	.wombat
	bgt	.wombat
	ble	.wombat
	bhi	.wombat
	blo	.wombat
	bul	.wombat

.back:
	bl	.local
	.space	(1 << 11)	@ leave space to force long offsets
.local:
	bl	.back
