	.text
	.thumb
	.syntax unified
thumb2_invert:
	cmp	r7, #0xffc00000
	cmn	r8, #0xffc00000
	add	r9, r4, #0xffc00000
	sub	r3, r6, #0xffc00000
	adc	r5, r0, #0x7fffffff
	sbc	r4, r7, #0x7fffffff
	and	r6, r2, #0x7fffffff
	bic	r8, r2, #0x7fffffff
	mov	r3, 0x7fffffff
	mvn	r1, 0x7fffffff
