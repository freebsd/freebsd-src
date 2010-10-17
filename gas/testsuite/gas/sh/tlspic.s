	.text
	.align 1
	.global	fn
	.type	fn, @function
fn:
	! Main binary, PIC
	mov.l	r12,@-r15
	mov.l	r14,@-r15
	mov	r15,r14
	mova	.L3,r0
	mov.l	.L3,r12
	add	r0,r12

	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	foo@GOTTPOFF
2:	! now r1 contains &foo

	mov	r1,r0
	mov	r14,r15
	mov.l	@r15+,r14
	rts	
	mov.l	@r15+,r12

	.align 2
.L3:	.long	_GLOBAL_OFFSET_TABLE_
