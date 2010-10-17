	.section .tbss,"awT",@nobits
	.global	x
y:	.space	4
x:	.space	4

	.section barfn,"ax",@progbits
	.align	1
	.type	bar, @function
bar:	
	mova	.L1,r0
	mov.l	.L1,r12
	add	r0,r12
	mov.l	1f,r0
	stc	gbr,r1
	mov.l	@(r0,r12),r0
	bra	2f
	add	r0,r1
	.align	2
1:	.long	x@GOTTPOFF
2:
	mov.l	@r1,r0
	rts	
	mov.l	@r15+,r12

	.align 2
.L1:	.long	_GLOBAL_OFFSET_TABLE_
