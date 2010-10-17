	;; Test all instructions in the m88k instruction set.
	;; Copyright 2001 Free Software Foundation, Inc.
	;; Contributed by Ben Elliston (bje at redhat.com).

.text
	;; integer add

	add	r0, r1, r2
	add.ci	r1, r2, r3
	add.co	r2, r3, r4
	add.cio	r3, r4, r5
	add	r4, r5, 0
	add	r4, r5, 4096

	;; unsigned integer add

	addu	 r0, r1, r2
	addu.ci	 r1, r2, r3
	addu.co	 r2, r3, r4
	addu.cio r3, r4, r5
	addu	 r4, r5, 0
	addu	 r4, r5, 4096

	;; logical and

	and	r0, r1, r2
	and.c	r1, r2, r3
	and	r2, r3, 0
	and	r2, r3, 4096
	and.u	r2, r3, 0
	and.u	r2, r3, 4096

	;; branch on bit clear

	bb0	0, r1, 0
	bb0	0, r1, -10
	bb0	0, r1, 10
	bb0	31, r1, 0
	bb0	31, r1, -10
	bb0	31, r1, 10
	bb0.n	0, r1, 0

	;; branch on bit set

	bb1	0, r1, 0
	bb1	0, r1, -10
	bb1	0, r1, 10
	bb1	31, r1, 0
	bb1	31, r1, -10
	bb1	31, r1, 10
	bb1.n	0, r1, 0

	;;  conditional branch

	bcnd	eq0, r1, 0
	bcnd	eq0, r1, 10
	bcnd	eq0, r1, -10	
	bcnd.n	eq0, r1, 0
	bcnd.n	eq0, r1, 10
	bcnd.n	eq0, r1, -10
	bcnd	ne0, r1, 0
	bcnd	ne0, r1, 10
	bcnd	ne0, r1, -10	
	bcnd.n	ne0, r1, 0
	bcnd.n	ne0, r1, 10
	bcnd.n	ne0, r1, -10
	bcnd	gt0, r1, 0
	bcnd	gt0, r1, 10
	bcnd	gt0, r1, -10	
	bcnd.n	gt0, r1, 0
	bcnd.n	gt0, r1, 10
	bcnd.n	gt0, r1, -10
	bcnd	lt0, r1, 0
	bcnd	lt0, r1, 10
	bcnd	lt0, r1, -10	
	bcnd.n	lt0, r1, 0
	bcnd.n	lt0, r1, 10
	bcnd.n	lt0, r1, -10
	bcnd	ge0, r1, 0
	bcnd	ge0, r1, 10
	bcnd	ge0, r1, -10	
	bcnd.n	ge0, r1, 0
	bcnd.n	ge0, r1, 10
	bcnd.n	ge0, r1, -10
	bcnd	le0, r1, 0
	bcnd	le0, r1, 10
	bcnd	le0, r1, -10	
	bcnd.n	le0, r1, 0
	bcnd.n	le0, r1, 10
	bcnd.n	le0, r1, -10
	;;  using m5 field
	bcnd	3, r1, 0
	bcnd	3, r1, 10
	bcnd	3, r1, -10	
	bcnd.n	3, r1, 0
	bcnd.n	3, r1, 10
	bcnd.n	3, r1, -10		

	;; uncoditional branch

	br 0
	br -10
	br 10
	br.n 0
	br.n -10
	br.n 10

	;; branch to subroutine

	bsr 0
	bsr -10
	bsr 10
	bsr.n 0
	bsr.n -10
	bsr.n 10
	
	;; clear bit field

	clr r1, r2, 5<15>
	clr r1, r2, r3
	clr r1, r2, 6
	clr r1, r2, <6>

	;; integer compare

	cmp r0, r1, r2
	cmp r0, r2, 0
	cmp r0, r2, 4096

	;; signed integer divide

	div r0, r1, r2
	div r0, r1, 0
	div r0, r1, 4096

	;; unsigned integer divide

	divu r0, r1, r2
	divu r0, r1, 0
	divu r0, r1, 10

	;; extract signed bit field

	ext r0, r1, 10<5>
	ext r1, r2, r3
	ext r2, r3, 6
	ext r2, r3, <6>

	;; extract unsigned bit field

	extu r0, r1, 10<5>
	extu r1, r2, r3
	extu r1, r2, 6
	extu r1, r2, <6>

	;; floating point add

	fadd.sss r0, r1, r2
	fadd.ssd r0, r1, r2
	fadd.sds r0, r1, r2
	fadd.sdd r0, r1, r2
	fadd.dss r0, r1, r2
	fadd.dsd r0, r1, r2
	fadd.dds r0, r1, r2
	fadd.ddd r0, r1, r2

	;; floating point compare

	fcmp.sss r0, r1, r2
	fcmp.ssd r0, r1, r2
	fcmp.sds r0, r1, r2
	fcmp.sdd r0, r1, r2

	;; floating point divide

	fdiv.sss r0, r1, r2
	fdiv.ssd r0, r1, r2
	fdiv.sds r0, r1, r2
	fdiv.sdd r0, r1, r2
	fdiv.dss r0, r1, r2
	fdiv.dsd r0, r1, r2
	fdiv.dds r0, r1, r2
	fdiv.ddd r0, r1, r2

	;; find first bit clear

	ff0 r1, r7

	;; find first bit set

	ff1 r3, r8

	;; load from floating-point control register

	fldcr r0, fcr50

	;; convert integer to floating point

	flt.ss r0, r3
	flt.ds r0, r10

	;; floating point multiply

	fmul.sss r0, r1, r2
	fmul.ssd r0, r1, r2
	fmul.sds r0, r1, r2
	fmul.sdd r0, r1, r2
	fmul.dss r0, r1, r2
	fmul.dsd r0, r1, r2
	fmul.dds r0, r1, r2
	fmul.ddd r0, r1, r2
	
	;; store to floating point control register

	fstcr r0, fcr50

	;; floating point subtract

	fsub.sss r0, r1, r2
	fsub.ssd r0, r1, r2
	fsub.sds r0, r1, r2
	fsub.sdd r0, r1, r2
	fsub.dss r0, r1, r2
	fsub.dsd r0, r1, r2
	fsub.dds r0, r1, r2
	fsub.ddd r0, r1, r2

	;; exchange floating point control register

	fxcr r0, r1, fcr50

	;; round floating point to integer

	int.ss r0, r1
	int.sd r10, r2

	;; unconditional jump

	jmp   r0
	jmp.n r10

	;; jump to subroutine

	jsr   r10
	jsr.n r13

	;; load register from memory

	;; unscaled
	ld.b    r0, r1, 0
	ld.b    r0, r1, 4096
	ld.bu   r0, r1, 0
	ld.bu   r0, r1, 4096
	ld.h    r0, r1, 0
	ld.h    r0, r1, 4096
	ld.hu	r0, r1, 0
	ld.hu	r0, r1, 4096
	ld	r0, r1, 0
	ld	r0, r1, 4096
	ld.d	r0, r1, 0
	ld.d	r0, r1, 4096
	;; unscaled
	ld.b	  r0, r1, r2
	ld.bu	  r1, r2, r3
	ld.h	  r2, r3, r4
	ld.hu 	  r3, r4, r5
	ld	  r4, r5, r6
	ld.d	  r5, r6, r7
	ld.b.usr  r6, r7, r8
	ld.bu.usr r7, r8, r9
	ld.h.usr  r8, r9, r1
	ld.hu.usr r9, r1, r2
	ld.usr	  r1, r2, r3
	ld.d.usr  r2, r3, r4
	;; scaled
	ld.b	  r0, r1[r2]
	ld.bu	  r1, r2[r3]
	ld.h	  r2, r3[r4]
	ld.hu 	  r3, r4[r5]
	ld	  r4, r5[r6]
	ld.d	  r5, r6[r7]
	ld.b.usr  r6, r7[r8]
	ld.bu.usr r7, r8[r9]
	ld.h.usr  r8, r9[r1]
	ld.hu.usr r9, r1[r2]
	ld.usr	  r1, r2[r3]
	ld.d.usr  r2, r3[r4]

	;; load address

	lda.h r0, r1[r2]
	lda   r1,r2[r3]
	lda.d r2,r3[r4]

	;; load from control register

	ldcr r0, cr10

	;; make bit field

	mak r0, r1, 10<5>
	mak r0, r1, r2
	mak r0, r1, 6
	mak r0, r1, <6>
	
	;; logical mask immediate

	mask   r0, r1, 0
	mask   r0, r1, 4096
	mask.u r0, r1, 0
	mask.u r0, r1, 4096

	;; integer multiply

	mul r0, r1, r2
	mul r0, r1, 0
	mul r0, r1, 4096

	;; floating point round to nearest integer

	nint.ss r0, r10
	nint.sd r10, r12

	;; logical or

	or   r0, r1, r2
	or.c r1, r7, r10
	or   r0, r4, 0
	or   r0, r4, 4096
	or.u r0, r1, 0
	or.u r2, r4, 4096

	;; rotate register

	rot r0, r1,<5>
	rot r2, r4, r6
	
	;; return from exception

	rte

	;; set bit field

	set r0, r1, 10<5>
	set r2, r4, r6
	set r3, r7, 6
	set r3, r7, <6>
	
	;; store register to memory

	;; unscaled
	st.b    r0, r1, 0
	st.b    r0, r1, 4096
	st.h    r0, r1, 0
	st.h    r0, r1, 4096
	st	r0, r1, 0
	st	r0, r1, 4096
	st.d	r0, r1, 0
	st.d	r0, r1, 4096
	;; unscaled
	st.b	  r0, r1, r2
	st.h	  r2, r3, r4
	st	  r4, r5, r6
	st.d	  r5, r6, r7
	st.b.usr  r6, r7, r8
	st.h.usr  r8, r9, r1
	st.usr	  r1, r2, r3
	st.d.usr  r2, r3, r4
	;; scaled
	st.b	  r0, r1[r2]
	st.h	  r2, r3[r4]
	st	  r4, r5[r6]
	st.d	  r5, r6[r7]
	st.b.usr  r6, r7[r8]
	st.h.usr  r8, r9[r1]
	st.usr	  r1, r2[r3]
	st.d.usr  r2, r3[r4]

	;; store to control register

	stcr r0, cr10

	;; integer subtract

	sub     r0, r1, r2
	sub.ci  r1, r2, r3
	sub.co  r2, r3, r4
	sub.cio r3, r4, r5
	sub     r4, r5, 0
	sub	r4, r5, 4096

	;; unsigned integer subtract

	subu	 r0, r1, r2
	subu.ci  r1, r2, r3
	subu.co  r3, r4, r5
	subu.cio r4, r5, r6
	subu	 r5, r6, 0
	subu	 r5, r6, 4096

	;; trap on bit clear

	tb0	0, r10, 10
	tb0	31, r11, 10

	;; trap on bit set

	tb1	0, r10, 10
	tb1	31, r11, 10

	;; trap on bounds check

	tbnd r0, r1
	tbnd r7, 0
	tbnd r7, 4096

	;; conditional trap

	tcnd  eq0, r10, 12
	tcnd  ne0, r9, 12
	tcnd  gt0, r8, 7
	tcnd  lt0, r7, 1
	tcnd  ge0, r6, 35
	tcnd  le0, r5, 33
	tcnd  10, r4, 12

	;; truncate floating point to integer

	trnc.ss r0, r1
	trnc.sd r1, r3

	;; exchange control register

	xcr r0, r3, cr10

	;; exchange register with memory

	;; FIXME: these should assemble!
	;; xmem.bu    r0, r1, 0
	;; xmem.bu     r0, r1, 10
	;; xmem	    r0, r1, 0
	;; xmem	    r1, r2, 4096
	xmem.bu     r0, r1, r2
	xmem	    r1, r2, r3
	xmem.bu.usr r4, r5, r6
	xmem.usr    r5, r6, r7
	xmem.bu     r2, r3[r4]
	xmem	    r3, r4[r5]
	xmem.bu.usr r4, r5[r9]
	xmem.usr    r5, r6[r10]

	;; logical exclusive or

	xor   r0, r1, r2
	xor.c r1, r2, r3
	xor   r2, r3, 0
	xor   r2, r4, 4096
	xor.u r1, r2, 0
	xor.u r2, r3, 4096
	
