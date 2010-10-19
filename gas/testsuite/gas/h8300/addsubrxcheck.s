	.section .text
	.global _main
_main:
	mov.w	r6,@-r7
	mov.w	r7,r6
	subs	#2,r7
	mov.w	@(-2,r6),r2
	subs	#2,r2
	mov.w	r2,@(-2,r6)
	sub.w	r2,r2
	mov.w	r2,r0
	adds	#2,r7
	mov.w	@r7+,r6
	rts
	.size	_main, .-_main
	.end
