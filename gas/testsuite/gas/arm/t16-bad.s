	@ Things you can't do with 16-bit Thumb instructions, but you can
	@ do with the equivalent ARM instruction.  Does not include errors
	@ caught by fixup processing (e.g. out-of-range immediates).

	.text
	.code 16
	.thumb_func
l:
	@ Arithmetic instruction templates
	.macro	ar2 opc
	\opc	r8,r0
	\opc	r0,r8
	.endm
	.macro	ar2sh opc
	ar2	\opc
	\opc	r0,#12
	\opc	r0,r1,lsl #2
	\opc	r0,r1,lsl r3
	.endm
	.macro	ar2r opc
	ar2	\opc
	\opc	r0,r1,ror #8
	.endm
	.macro 	ar3 opc
	\opc	r1,r2,r3
	\opc	r8,r0
	\opc	r0,r8
	.endm
	.macro ar3sh opc
	ar3	\opc
	\opc	r0,#12
	\opc	r0,r1,lsl #2
	\opc	r0,r1,lsl r3
	.endm

	ar2sh	tst
	ar2sh	cmn
	ar2sh	mvn
	ar2	neg
	ar2	rev
	ar2	rev16
	ar2	revsh
	ar2r	sxtb
	ar2r	sxth
	ar2r	uxtb
	ar2r	uxth

	ar3sh	adc
	ar3sh	and
	ar3sh	bic
	ar3sh	eor
	ar3sh	orr
	ar3sh	sbc
	ar3	mul

	@ Shift instruction template
	.macro	shift opc
	\opc	r8,r0,#12  @ form 1
	\opc	r0,r8,#12
	ar2	\opc	   @ form 2
	.endm
	shift	asr
	shift	lsl
	shift	lsr
	shift	ror
	ror	r0,r1,#12

	@ add/sub/mov/cmp are idiosyncratic
	add	r0,r1,lsl #2
	add	r0,r1,lsl r3
	add	r8,r0,#1	@ form 1
	add	r0,r8,#1
	add	r8,#10		@ form 2
	add	r8,r1,r2	@ form 3
	add	r1,r8,r2
	add	r1,r2,r8
	add	r8,pc,#4	@ form 5
	add	r8,sp,#4	@ form 6

	ar3sh	sub
	sub	r8,r0,#1	@ form 1
	sub	r0,r8,#1
	sub	r8,#10		@ form 2
	sub	r8,r1,r2	@ form 3
	sub	r1,r8,r2
	sub	r1,r2,r8

	cmp	r0,r1,lsl #2
	cmp	r0,r1,lsl r3
	cmp	r8,#255

	mov	r0,r1,lsl #2
	mov	r0,r1,lsl r3
	mov	r8,#255

	@ Load/store template
	.macro	ldst opc
	\opc	r8,[r0]
	\opc	r0,[r8]
	\opc	r0,[r0,r8]
	\opc	r0,[r1,#4]!
	\opc	r0,[r1],#4
	\opc	r0,[r1,-r2]
	\opc	r0,[r1],r2
	.endm
	ldst	ldr
	ldst	ldrb
	ldst	ldrh
	ldst	ldrsb
	ldst	ldrsh
	ldst	str
	ldst	strb
	ldst	strh

	ldr	r0,[r1,r2,lsl #1]
	str	r0,[r1,r2,lsl #1]
	
	@ Load/store multiple
	ldmia	r8!,{r1,r2}
	ldmia	r7!,{r8}
	ldmia	r7,{r1,r2}
	ldmia	r7!,{r1,r7}

	stmia	r8!,{r1,r2}
	stmia	r7!,{r8}
	stmia	r7,{r1,r2}
	stmia	r7!,{r1,r7}

	push	{r8,r9}
	pop	{r8,r9}

	@ Miscellaneous
	bkpt	#257
	cpsie	ai,#5
	cpsid	ai,#5

	@ Conditional suffixes
	addeq	r0,r1,r2
