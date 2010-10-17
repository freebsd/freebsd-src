! { dg-do assemble }
! { dg-options "-dsp" }

	.text
	.p2align 2

	ldrc	a0			! { dg-error "invalid operand" }
	
	movx.w	@r3,x0			! { dg-error "invalid operand" }
	movx.w	@r0,x0	movy.w	a0,@r7+	! { dg-error "requires nopy" }
	movy.w	a0,@r2+	movx.w	@r4,x0	! { dg-error "requires nopx" }
	movx.w	@r4,x0	movy.w	a0,@r3+	! { dg-error "combined with non-nopx" }
	movy.w	a0,@r6+	movx.w	@r1,x0	! { dg-error "combined with non-nopy" }
	movx.l	@r5,x1	movx.w	@r0,x0	! { dg-error "multiple movx" }
	movx.l	@r1+,y0	nopx		! { dg-error "multiple movx" }
	movy.w	@r7,y1	movy.l	@r2,y0	! { dg-error "multiple movy" }
	movy.l	@r3+,x0	nopy		! { dg-error "multiple movy" }
	
    dct	pclr	x0	pmuls a1,x0,m0	! { dg-error "combined with pmuls" }
	pclr	a0	pmuls x1,y1,a0	! { dg-warning "register is same" }
