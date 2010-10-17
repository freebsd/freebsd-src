	.text
	.align

	mova	.Lgot, r0
	mov.l	.Lgot, r12
	add	r0, r12
	mov.l	.Lfoogot, r0
	mov.l	@(r0,r12), r0
	jsr	@r0
	nop
	mov.l	.Lfoogotoff, r0
	add	r12, r0
	jsr	@r0
	nop
	mov.l	.Lfooplt, r1
	mova	.Lfooplt, r0
	add	r1, r0
	jsr	@r0
	nop
	mov.l	.Lfooplt_old, r0
	jsr	@r0
.LPLTcall_old:
	nop
	mov.l	.Lfooplt_new, r0
	jsr	@r0
.LPLTcall_new:
	nop
	
	.p2align 2
.Lgot:	
	.long	GLOBAL_OFFSET_TABLE
.Lfoogot:
	.long	foo@GOT
.Lfoogotoff:
	.long	foo@GOTOFF
.Lfooplt:
	.long	foo@PLT
.Lfooplt_old:
	.long	foo@PLT + . - (.LPLTcall_old + 2)
.Lfooplt_new:
	.long	foo@PLT - (.LPLTcall_new + 2 - .)
.Lfooplt_old2:
	.long	foo@PLT + . - 2 - .LPLTcall_old
