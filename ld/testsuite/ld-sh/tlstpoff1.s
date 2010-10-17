	.text
	.align 5
	.global	foo
	.type	foo, @function
foo:
	mov.l	r12,@-r15
	mova	.L1,r0
	mov.l	.L1,r12
	add	r0,r12
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	 add	r0,r1
	.align 2
1:	.long	x@GOTTPOFF
2:	
	mov.l	@r1,r0
	rts	
	mov.l	@r15+,r12

	.align 2
.L1:	.long	_GLOBAL_OFFSET_TABLE_
