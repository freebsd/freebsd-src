	.text

	.p2align 2
code:
	mov.l	litpool, r1
	mov.l	@(14,pc), r1
	mova	@(litpool-.,pc), r0
	mov.l	@r0,r1
	mov.l	@(litpool-.,pc), r1
	bsrf	r1
	nop
	nop
litpool:
	.long	code - .
