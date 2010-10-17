! { dg-do assemble }

! Various operand errors experienced during the creation of basic-1.s;
! some are redundant.
!
	addz.l	r51,-42,r30		! { dg-error "invalid operand" }
	beqi	r4,-33,tr5		! { dg-error "not a 6-bit signed value" }
	fadd.s	dr41,dr59,dr19		! { dg-error "invalid operand" }
	fdiv.s	fr13,dr26,fr19		! { dg-error "invalid operand" }
	fld.p	r53,-3000,fp39		! { dg-error "invalid operand" }
	fld.s	r53,1010,fr53		! { dg-error "not a multiple of 4" }
	float.qd	dr45,dr16	! { dg-error "invalid operand" }
	float.qs	dr31,fr11	! { dg-error "invalid operand" }
	fmov.d	dr8,dr43		! { dg-error "invalid operand" }
	fmov.qd	r45,dr5			! { dg-error "invalid operand" }
	fmul.d	dr7,dr57,dr42		! { dg-error "invalid operand" }
	fneg.s	fr0,dr33		! { dg-error "invalid operand" }
	fsqrt.d	dr31,dr43		! { dg-error "invalid operand" }
	fst.p	r54,-4008,fp11		! { dg-error "invalid operand" }
	fstx.p	r38,r26,dr52		! { dg-error "invalid operand" }
	ftrc.dq	dr15,dr29		! { dg-error "invalid operand" }
	ftrv.s	mtrx16,fv32,fv7		! { dg-error "invalid operand" }
	icbi	r48,12000		! { dg-error "not a 11-bit signed value" }
	ld.w	r46,301,r11		! { dg-error "not an even value" }
	ldhi.l	r6,302,r41		! { dg-error "not a 6-bit signed value" }
	ldlo.l	r19,334,r48		! { dg-error "not a 6-bit signed value" }
	ldlo.q	r9,311,r29		! { dg-error "not a 6-bit signed value" }
	ocbi	r43,11008		! { dg-error "not a 11-bit signed value" }
	ocbp	r40,-11008		! { dg-error "not a 11-bit signed value" }
	ocbwb	r44,-10016		! { dg-error "not a 11-bit signed value" }
	prefi	r57,16000		! { dg-error "not a 11-bit signed value" }
	putcfg	r41,-511,r62		! { dg-error "not a 6-bit signed value" }
	shlld	r56,38,r23		! { dg-error "invalid operand" }
	shlli.l	r61,r35,r26		! { dg-error "invalid operand" }
	shlli	r60,r36,r25		! { dg-error "invalid operand" }
	shlri	r2,r32,r29		! { dg-error "invalid operand" }
	shlri.l	r3,r31,r30		! { dg-error "invalid operand" }
	st.w	r9,2002,r33		! { dg-error "not a 11-bit signed value" }
	sthi.l	r10,-201,r43		! { dg-error "not a 6-bit signed value" }
	sthi.q	r12,203,r44		! { dg-error "not a 6-bit signed value" }
	stlo.l	r13,-207,r45		! { dg-error "not a 6-bit signed value" }
	stlo.q	r15,217,r46		! { dg-error "not a 6-bit signed value" }
	stx.b	r16,219,r47		! { dg-error "invalid operand" }
	stx.l	r17,-500,r48		! { dg-error "invalid operand" }
	stx.q	r19,-50,r49		! { dg-error "invalid operand" }
	stx.w	r20,-150,r50		! { dg-error "invalid operand" }
	xori	r29,-51,r55		! { dg-error "not a 6-bit signed value" }
