	@ ARM instructions defined for source compatibility with Thumb.
	.macro	shift op opls ops oplss
	\oplss	r9,r0
	\opls	r0,r0,r9
	\ops	r0,#17
	\op	r0,r9,#17
	.endm
	.text
	.global l
l:
	cpyls	r0,r0
	cpy	r9,r0
	cpy	r0,r9
	cpy	ip,lr

	shift	lsl lslls lsls lsllss
	shift	lsr lsrls lsrs lsrlss
	shift	asr asrls asrs asrlss
	shift	ror rorls rors rorlss

	neg	r0,r9
	negs	r9,r0
	negls	r0,r0
	neglss	r9,r9

	push	{r1,r2,r3}
	pushls	{r2,r4,r6,r8,pc}
	pop	{r1,r2,r3}
	popls	{r2,r4,r6,r8,pc}

	@ Two-argument forms of ARM arithmetic instructions.
	and	r0,r1
	eor	r0,r1
	sub	r0,r1
	rsb	r0,r1

	add	r0,r1
	adc	r0,r1
	sbc	r0,r1
	rsc	r0,r1

	orr	r0,r1
	bic	r0,r1
	mul	r0,r1
	nop
