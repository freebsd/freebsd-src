! Verify that minimum support is provided as per SH-5/ST50-047-02.

	.text
	.mode shmedia
start:

! Both all-upper and all-lower must be supported.  PTA/PTB without /L
! or /U is equivalent to with /L

	ld.l	r1,4,r1
	LD.L	r1,4,r1
	pta/l	.L1,tr0
	PTA/L	.L1,tr0
	pta/u	.L1,tr0
	PTA/U	.L1,tr0
	pta/l	.L1,tr0
	PTA/L	.L1,tr0
	ptb/u	.L2,tr0
	PTB/U	.L2,tr0
	ptb/l	.L2,tr0
	PTB/L	.L2,tr0
	pta	.L1,tr0
	PTA	.L1,tr0
	ptb	.L2,tr0
	PTB	.L2,tr0
.L1:
	.mode shcompact
.L2:
	.align 2
	.mode shmedia

! All register names accepted, either case.

	or	r0,r32,r63
	GETCON	CR0,R0
	getcon	cr63,r0
	GETTR	TR0,R0
	gettr	tr7,r0
	FMOV.S	FR0,FR63
	fmov.d	dr0,dr62
	FTRV.S	MTRX0,FV0,fv0
	ftrv.s	mtrx48,FV60,FV60

! All control register names

	getcon	sr,r0
	getcon	ssr,r0
	getcon	pssr,r0
	getcon	intevt,r0
	getcon	expevt,r0
	getcon	pexpevt,r0
	getcon	tra,r0
	getcon	spc,r0
	getcon	pspc,r0
	getcon	resvec,r0
	getcon	vbr,r0
	getcon	tea,r0
	getcon	dcr,r0
	getcon	kcr0,r0
	getcon	kcr1,r0
	getcon	ctc,r0
	getcon	usr,r0

! immediates

	.mode shcompact
s1:
	mov	#4,r0

	.align 2
	.mode shmedia
s2:
	movi	4,r0

! Scaled operands - user gives scaled value

	.mode shcompact
s3:
	mov.l	@(8,r0),r0

	.align 2
	.mode shmedia
s4:
	ld.uw	r0,2,r0
	ld.w	r0,2,r0
	st.w	r0,2,r0
	ld.l	r0,4,r0
	st.l	r0,4,r0
	fld.s	r0,4,fr0
	fst.s	r0,4,fr0
	pta	.+4,tr0
	ptb	.+7,tr0
	ld.q	r0,8,r0
	st.q	r0,8,r0
	fld.d	r0,8,dr0
	fst.d	r0,8,dr0
	fld.p	r0,8,fp0
	fst.p	r0,8,fp0
	alloco	r0,32
	icbi	r0,32
	ocbi	r0,32
	ocbp	r0,32
	ocbwb	r0,32
	prefi	r0,32

	.mode	shcompact
s5:
	mov.w	@(6,pc),r0
	mov.w	@(2,r0),r0
	mov.w	@(2,gbr),r0
	mov.w	r0,@(2,r0)
	mov.w	r0,@(2,gbr)
	bf	.+6
	bt	.+6
	bra	.+6
	bsr	.+6
	mov.l	@(4,pc),r0
	mov.l	@(4,r0),r0
	mov.l	@(4,gbr),r0
	mova	@(6,pc),r0
	mov.l	r0,@(4,r0)
	mov.l	r0,@(4,gbr)

! branchlabel vs datalabel

	.align 2
	.mode shmedia
s6:
	.long	.L3
	.long	.L3 + 4
	.long	datalabel .L3
	.long	DATALABEL .L3
.L3:
	.mode	shcompact

s7:
	.long	.L4
	.long	.L5
.L4:

	.align 2
	.mode shmedia
s8:

	movi	(.L4 >> 16) & 65535,r0
	shori	.L4 & 65535, r0
	ptabs	r0,tr0
	blink	tr0,r18

	movi	(.L5 >> 16) & 65535,r0
	shori	.L5 & 65535, r0
	ptabs	r0,tr0
	blink	tr0,r18
.L5:

	movi	(.L4-DATALABEL .L6), r0
.L6:
	movi	(.L5-DATALABEL .L7), r0
.L7:

	pt	.L5,tr0

! Expressions

! Symbols

abcdefghijklmnopqrstuvwxyz0123456789_:
	.long	abcdefghijklmnopqrstuvwxyz0123456789_
_x:
	.long	_x

! program counter

	movi	.L7-$,r0
.L8:	movi	.L7-.L8,r0

	.mode shcompact
s9:
	mova	@(litpool-$,pc), r0
	mov.l	@r1,r0
	add	r1,r0
	bsrf	r0
litpool:
	.long	s10 - DATALABEL $

! operators

	.align 2
	.mode shmedia
s10:
	movi	8+1,r0
	movi	8-1,r0
	movi	8<<1,r0
	movi	8>>1,r0
	movi	~1,r0
	movi	5&3,r0
	movi	8|1,r0
	movi	8*3,r0
	movi	24/3,r0
