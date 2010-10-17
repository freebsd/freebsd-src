	.text
	.p2align 2

	ldrc	r3
	ldrc	r12
	ldrc	#10
	ldrc	#243
	setdmx
	setdmy
	clrdmxy

	movx.w	@r4,x0		movy.w	a0,@r7+
	movx.w	@r0,x1
	movx.w	@r1+,y0		nopy
nopy	movx.w	@r5+r8,y1

	movx.l	@r5,x1
	movx.l	@r0,x0
	movx.l	@r1+,y0		nopy
nopy	movx.l	@r4+r8,y1

	movx.w	a0,@r4+		movy.w	@r6+r9,y0
	movx.w	x0,@r1
	movx.w	a1,@r0+		nopy
nopy	movx.w	x1,@r5+r8

	movx.l	a0,@r5
	movx.l	x0,@r0
	movx.l	x1,@r1+		nopy
nopy	movx.l	a1,@r4+r8

	movy.w	@r7,y1		movx.w a1,@r4+r8
	movy.w	@r3,y0
	movy.w	@r2+,x1		nopx
nopx	movy.w	@r6+r9,x0

	movy.l	@r6,y1
	movy.l	@r2,y0
	movy.l	@r3+,x0		nopx
nopx	movy.l	@r7+r9,x1

	movy.w	a1,@r6+		movx.w	@r5+r8,x1
	movy.w	y1,@r2
	movy.w	a0,@r3+		nopx
nopx	movy.w	y0,@r7+r9

	movy.l	a1,@r7
	movy.l	y0,@r3
	movy.l	y1,@r2+		nopx
nopx	movy.l	a0,@r6+r9

	pabs	x1,a0
	pabs	y0,m1
    dct	pabs	a1,m0
    dct	pabs	x0,x1
    dcf	pabs	a0,y1
    dcf	pabs	x1,a0
    dct	pabs	y1,x0
    dct	pabs	m0,m1
    dcf	pabs	m1,y0
    dcf	pabs	y0,a1

	pmuls	a1,x0,m0
	pmuls	y0,a1,m1
	pclr	a0
    dct	pclr	a1
	pclr	x0		pmuls	a1,x0,m0
	pclr	a1		pmuls	x0,y0,a0
	pclr	a0		pmuls	x1,y1,a1
	pclr	y0		pmuls	y0,a1,m1

	psub	a0,m0,x0
	psub	m1,x1,x1
	psub	y0,a0,y0
    dct	psub	a1,y1,y1
    dct	psub	m0,x1,a0
    dct	psub	y1,a0,a1
    dcf	psub	x1,m1,m0
    dcf	psub	y0,x1,m1
    dcf	psub	m1,a0,a1

	pswap	a1,m1
	pswap	x0,a0
	pswap	m1,y0
	pswap	y0,x1
    dct	pswap	a0,y1
    dct	pswap	x1,x0
    dct	pswap	y1,a1
    dct	pswap	m0,m0
    dcf	pswap	a0,a0
    dcf	pswap	x1,m1
    dcf	pswap	m1,x0
    dcf	pswap	y0,y1

	prnd	a0,a1
	prnd	y1,m0
    dct	prnd	a1,x0
    dct	prnd	x0,y1
    dct	prnd	m1,a0
    dct	prnd	y0,x1
    dcf	prnd	a0,y0
    dcf	prnd	x1,m1
    dcf	prnd	y1,a0
    dcf	prnd	m0,a1
