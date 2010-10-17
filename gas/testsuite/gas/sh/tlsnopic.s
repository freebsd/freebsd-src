	.section	.tbss,"awT",@nobits
	.align 2
	.long	foo
	.text
	.align 1
	.global	fn
	.type	fn, @function
fn:
	! Main binary, no PIC
	mov.l	r14,@-r15
	mov	r15,r14

	stc	gbr,r1
	mov.l	.L2,r0
	add	r1,r0
	! r0 now contains &foo

	mov	r14,r15
	rts	
	mov.l	@r15+,r14
.L3:
	.align 2
.L2:	.long	foo@TPOFF
