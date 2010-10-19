	.text

# New instructions

	band.b	#7,@(4095,r3)

	bandnot.b	#7,@(4095,r3)

	bclr.b	#7,@(4095,r3)
	bclr	#7,r3

	bld.b	#7,@(4095,r3)
	bld	#7,r3

	bldnot.b	#7,@(4095,r3)

	bor.b	#7,@(4095,r3)

	bornot.b	#7,@(4095,r3)

	bset.b	#7,@(4095,r3)
	bset	#7,r3

	bst.b	#7,@(4095,r3)
	bst	#7,r3

	bxor.b	#7,@(4095,r3)

	clips.b	r3
	clips.w	r3
	clipu.b	r3
	clipu.w	r3

	divs	r0,r3
	divu	r0,r3

	fmov.s	fr3,@(4095*4,r3)
	fmov.d	dr2,@(4095*8,r3)
	fmov.s	@(4095*4,r3),fr3
	fmov.d	@(4095*8,r3),dr2

	jsr/n	@r3
	jsr/n	@@(255*4,tbr)

	ldbank	@r3,r0

	ldc	r3,tbr

	mov.b	r3,@(4095,r4)
	mov.w	r3,@(4095*2,r4)
	mov.l	r3,@(4095*4,r4)
	mov.b	@(4095,r4),r5
	mov.w	@(4095*2,r4),r5
	mov.l	@(4095*4,r4),r5

	mov.b	r0,@r3+
	mov.w	r0,@r3+
	mov.l	r0,@r3+
	mov.b	@-r3,r0
	mov.w	@-r3,r0
	mov.l	@-r3,r0

	movi20	#524287,r3
	movi20	#-524288,r3
	movi20s	#524287*256,r3
	movi20s	#-524288*256,r3

	movml.l	r3,@-r15
	movml.l	@r15+,r3

	movmu.l	r3,@-r15
	movmu.l	@r15+,r3

	movrt	r3

	movu.b	@(4095,r3),r4
	movu.w	@(4095*2,r3),r4

	mulr	r0,r4

	nott

	pref	@r5

	resbank

	rts/n

	rtv/n	r3

	shad	r3,r4
	shld	r3,r4

	stbank	r0,@r5

	stc	tbr,r4
