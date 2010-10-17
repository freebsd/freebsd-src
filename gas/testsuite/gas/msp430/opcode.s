	.arch msp430x123
	.text
	.p2align 1,0
	
.global	foo
foo:
	and	#1, r11
	inv	r10
	xor	#0x00ff, r11
	bis	#8,	r12
	bit	#0x10,	r13
	bic	#0xa0, r14
	cmp	#0, r15
	sub	#1, r10
	subc	#0, r11
	add	#1, r12
	addc	#2, r13
	push	r14
	pop	r15
	sxt	r10
	rra	r11
	swpb	r12
	rrc	r13
	ret

	.p2align 1,0
.global	main
	.type	main,@function
main:
	mov	#(__stack-0), r1
	call	#foo
	mov	&a, r14
	mov	r14, r15
	rla	r15
	subc	r15, r15
	inv	r15
	call	#__floatsisf
	mov	r14, &c
	mov	r15, &c+2
	mov	&b, r14
	mov	r14, r15
	rla	r15
	subc	r15, r15
	inv	r15
	call	#__floatsisf
	mov	r14, &d
	mov	r15, &d+2
	mov	#llo(240), r15 
	br	#__stop_progExec__
	.comm a,2,2
	.comm b,2,2
	.comm c,4,2
	.comm d,4,2

	;; This next instruction triggered a bug which
	;; was fixed by a patch to msp430-dis.c on Jan 2, 2004
	add	&0x200, &0x172
