	.file	"too_large.c"
	.text
	nop
	.align 8
.L307:
	.byte	.L302-.L307
	.byte	.L303-.L307
	.byte	.L304-.L307
	.byte	.L305-.L307
.L304:
	mov.l	.L318,r1	
	jsr	@r1	
	mov	r8,r4	
	lds	r0,fpul	
	fsts	fpul,fr1	
	flds	fr1,fpul	
	sts	fpul,r0	
	mov	r14,r15	
	lds.l	@r15+,pr	
	mov.l	@r15+,r14	
	mov.l	@r15+,r8	
	rts	
	nop	
.L305:
	mov.l	.L319,r7	
	jsr	@r7	
	mov	r8,r4	
	lds	r0,fpul	
	bra	.L307	
	fsts	fpul,fr1	
.L303:
	mov.l	.L320,r6	
	jsr	@r6	
	mov	r8,r4	
	lds	r0,fpul	
	bra	.L307	
	fsts	fpul,fr1	
.L302:
	mov.l	.L321,r5	
