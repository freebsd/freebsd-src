	.h8300h
	.section	.text.functionWeUse,"ax",@progbits
	.align 1
	.global _functionWeUse
_functionWeUse:
	mov.l	er6,@-er7
	mov.l	er7,er6
	subs	#4,er7
	mov.w	r0,@(-2,er6)
	mov.w	@(-2,er6),r2
	mov.w	r2,r0
	adds	#4,er7
	mov.l	@er7+,er6
	rts
	.size	_functionWeUse, .-_functionWeUse
	.section	.text.functionWeDontUse,"ax",@progbits
	.align 1
	.global _functionWeDontUse
_functionWeDontUse:
	mov.l	er6,@-er7
	mov.l	er7,er6
	subs	#4,er7
	mov.w	r0,@(-2,er6)
	mov.w	@(-2,er6),r2
	mov.w	r2,r0
	adds	#4,er7
	mov.l	@er7+,er6
	rts
	.size	_functionWeDontUse, .-_functionWeDontUse
	.section	.text.start,"ax",@progbits
	.align 1
	.global _start
_start:
	mov.l	er6,@-er7
	mov.l	er7,er6
	mov.w	#75,r0
	jsr	@_functionWeUse
	mov.w	r0,r2
	mov.w	r2,r0
	mov.l	@er7+,er6
	rts
	.size	_start, .-_start
	.end
