	.text
	.globl	_foo
	.type	_foo, %function
_foo:
	mov.l	r12,@-r15
	sts.l	pr,@-r15
	mov.l	1f,r12
	mov.l	@r12,r12
	mov.l	2f,r0
	mov.l	@(r0,r12),r12

	mov.l	3f,r0
	mov.l	@(r0,r12),r1
	mov.l	@r1,r2
	add	#1,r2
	mov.l	r2,@r1

	mov.l	4f,r0
	bsrf	r0
	nop
.Lb4:

	mov.l	5f,r0
	bsrf	r0
	nop
.Lb5:

	mov.l	6f,r0
	bsrf	r0
	nop
.Lb6:

	lds.l	@r15+,pr
	rts
	mov.l	@r15+,r12
	.align	2
1:	.long	___GOTT_BASE__
2:	.long	___GOTT_INDEX__
3:	.long	x@GOT
4:	.long	_slocal - .Lb4
5:	.long	_sglobal@PLT - (.Lb5 - .)
6:	.long	_sexternal@PLT - (.Lb6 - .)
	.size	_foo, .-_foo

	.type	_slocal, %function
_slocal:
	rts
	nop
	.size	_slocal, .-_slocal

	.globl	_sglobal
	.type	_sglobal, %function
_sglobal:
	rts
	nop
	.size	_sglobal, .-_sglobal

	.data
	.4byte	_slocal

	.comm	x,4,4
