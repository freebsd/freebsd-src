	@ Three-argument forms of Thumb arithmetic instructions.
	@ Commutative instructions allow either the second or third
	@ operand to equal the first.

	.text
	.global m
	.thumb_func
m:
	adc	r0,r0,r1
	adc	r0,r1,r0

	and	r0,r0,r1
	and	r0,r1,r0

	eor	r0,r0,r1
	eor	r0,r1,r0

	mul	r0,r0,r1
	mul	r0,r1,r0

	orr	r0,r0,r1
	orr	r0,r1,r0

	bic	r0,r0,r1

	sbc	r0,r0,r1

	@ section padding for a.out's sake
	nop
	nop
	nop
	nop
